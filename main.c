#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "usb_mouse.h"
#if SROM_VERSION == 3
#   include "srom_3360_0x03.h"
#elif SROM_VERSION == 5
#   include "srom_3360_0x05.h"
#else
#   error bad srom version
#endif

#include <stdbool.h>

#include "mouse.h"
#include "buttons.h"

#define delay_us(t) __builtin_avr_delay_cycles((t) * F_CPU/1000000)
#define delay_ms(t) __builtin_avr_delay_cycles((t) * F_CPU/1000)


#define PORT_SPI PORTB
#define DDR_SPI	DDRB

#define DD_SS	6 // aka NCS
#define DD_SCK	1
#define DD_MOSI	2
#define DD_MISO	3

#define SS_LOW	(PORT_SPI &= ~(1<<DD_SS))
#define SS_HIGH	(PORT_SPI |= (1<<DD_SS))

union motion_data {
	int16_t all;
	struct { uint8_t lo, hi; };
};

static void pins_init(void)
{
	DDRC |= _BV(PC2);
	PORTC |= _BV(PC2);
	/* initialize buttons inputs */
	DDRD &= ~(_BV(PD3) | _BV(PD2) | _BV(PD1) | _BV(PD0));
	PORTD |= _BV(PD3) | _BV(PD2) | _BV(PD1) | _BV(PD0);
#ifdef DEBUG_PINS
	DDRB |= _BV(PB5);
	PORTB &= ~_BV(PB5);
	DDRC |= _BV(PC6);
	PORTC &= ~_BV(PC6);
#endif
}


static void spi_init(void)
{
	DDR_SPI |= (1<<DD_MOSI) | (1<<DD_SCK) | (1<<DD_SS); // outputs
	DDRB |= (1<<0); PORTB |= (1<<0); // set the hardware SS pin to low to enable SPI
	// MISO pullup input is already done in hardware
	// enable spi, master mode, mode 3, clock rate = fck/4 = 2MHz
	SPCR = (1<<SPE) | (1<<MSTR) | (1<<CPOL) | (1<<CPHA);
}

static inline void spi_send(const uint8_t b)
{
	SPDR = b;
	while (!(SPSR & (1<<SPIF)));
}

static inline uint8_t spi_recv(void)
{
	spi_send(0x00);
	return SPDR;
}

static inline void spi_write(const uint8_t addr, const uint8_t data)
{
	spi_send(addr | 0x80);
	spi_send(data);
	delay_us(180); // maximum of t_SWW, t_SWR
}

static inline uint8_t spi_read(const uint8_t addr)
{
	spi_send(addr);
	delay_us(160); // t_SRAD
	uint8_t data = spi_recv();
	delay_us(20);
	return data;
}

static void pmw3366_init(const uint8_t dpi)
{
	const uint8_t *psrom = srom;

	SS_HIGH;
	delay_ms(3);

	// shutdown first
	SS_LOW;
	spi_write(0x3b, 0xb6);
	SS_HIGH;
	delay_ms(300);

	// drop and raise ncs to reset spi port
	SS_LOW;
	delay_us(40);
	SS_HIGH;
	delay_us(40);

	// power up reset
	SS_LOW;
	spi_write(0x3a, 0x5a);
	SS_HIGH;
	delay_ms(50);

	// read from 0x02 to 0x06
	SS_LOW;
	spi_read(0x02);
	spi_read(0x03);
	spi_read(0x04);
	spi_read(0x05);
	spi_read(0x06);

	spi_write(0x10, 0x00);
	spi_write(0x22, 0x00);

	// srom download
	spi_write(0x13, 0x1d);
	SS_HIGH;
	delay_ms(10);
	SS_LOW;
	spi_write(0x13, 0x18);

	spi_send(0x62 | 0x80);
	for (uint16_t i = 0; i < SROM_LENGTH; i++) {
		delay_us(16);
		spi_send(pgm_read_byte(psrom++));
	}
	delay_us(18);
	SS_HIGH;
	delay_us(200);

	// check srom id
	SS_LOW;

	// configuration/settings
	spi_write(0x10, 0x00); // 0x20 (g502 default) enables rest mode after ~10s of inactivity
	spi_write(0x14, 0xff); // how long to wait before going to rest mode. 0xff is max (~10 seconds)
	spi_write(0x17, 0xff);
	spi_write(0x18, 0x00);
	spi_write(0x19, 0x00);
	spi_write(0x1b, 0x00);
	spi_write(0x1c, 0x00);

	spi_write(0x2c, 0x0a);
	spi_write(0x2b, 0x10);

	// configuration/settings
	spi_write(0x0f, dpi); // 2000 dpi
	spi_write(0x42, 0x00); // no angle snapping
	spi_write(0x0d, 0x60); // invert x,y
	SS_HIGH;
}

#define CPI_VAL(cpi) ((cpi) / 100 - 1)

void pmw3366_set_cpi(int16_t cpi)
{
	static int16_t last_cpi = 0;
	if (cpi != last_cpi) {
		SS_LOW;
		spi_write(0x0f, CPI_VAL(cpi));
		SS_HIGH;
		last_cpi = cpi;
	}
}
void pmw3366_set_mode(bool as, int8_t lod)
{
	static int8_t last_as = -1;
	static int8_t last_lod = 0;
	if ((int8_t)as == last_as && lod == last_lod)
		return;
	SS_LOW;
	if ((int8_t)as != last_as) {
		spi_write(0x42, as ? (1 << 7) : 0);
		last_as = (int8_t)as;
	}
	if (lod != last_lod) {
		spi_write(0x63, lod);
		last_lod = lod;
	}
	SS_HIGH;
}

int main(void)
{
	union motion_data x, y;

	// set clock prescaler for 8MHz
	CLKPR = 0x80;
	CLKPR = 0x01;

	pins_init();

	// previous state to compare against for debouncing
	uint8_t btn_prev = 0x00;
	// binary OR of all button states since previous usb transmission
	uint8_t btn_usb = 0x00;
	// previously transmitted button state
	uint8_t btn_usb_prev = 0x00;

	spi_init();
	const uint8_t dpi = ((PIND & (1<<6)) >> 6) | ((PIND & (1<<4)) >> 3);
	const uint8_t dpis[] = {CPI_VAL(1500), CPI_VAL(500), CPI_VAL(600), CPI_VAL(700)};
	pmw3366_init(dpis[dpi]);

	usb_init();
	while (!usb_configured())
		;

	// begin burst mode
	SS_LOW;
	spi_write(0x50, 0x00);
	SS_HIGH;

	// set up timer0 to set OCF0A in TIFR0 every 125us
	TCCR0A = 0x02; // CTC
	TCCR0B = 0x02; // prescaler 1/8 = 1us period
	OCR0A = 124; // = 125 - 1

	BUTTONS_set_debounce_delay(160);

	uint32_t time_ticks = 0;

	uint8_t prev_squal = 128;
	while (1) {
		for (uint8_t i = 0; i < 8; i++) {
			if (i == 0) {
				// sync to usb frames (1ms)
				UDINT &= ~(1<<SOFI);
				while(!(UDINT & (1<<SOFI)));
				// reset prescaler phase, not really necessary
				GTCCR |= (1<<PSRSYNC);
				TCNT0 = 0;
			} else {
				// sync to 125us intervals using timer0
				while (!(TIFR0 & (1<<OCF0A)));
			}
#ifdef DEBUG_PINS
			PORTB |= _BV(PB5);
			PORTB &= ~_BV(PB5);
#endif

			TIFR0 |= (1<<OCF0A); // 0CF0A is cleared by writing 1

			const uint8_t intr_state = SREG;
			cli();

			union motion_data _x, _y;

			SS_LOW;
			spi_send(0x50);

			/* instead of delay..., this should take more than 35ms */
			bool overwrite_delta;
			struct input inputs[2] = {
				{.T = !(PIND & _BV(PD2)), .B = !(PIND & _BV(PD0))},
				{.T = !(PIND & _BV(PD3)), .B = !(PIND & _BV(PD1))}
			};
			BUTTONS_task(1, inputs);
			bool b1 = BUTTONS_get(0), b2 = BUTTONS_get(1);
			int16_t out_dx, out_dy;
			overwrite_delta = mouse_step(time_ticks, b1, b2, prev_squal >= 16,
					&b1, &b2, &out_dx, &out_dy);

#ifdef DEBUG_PINS
			PORTC |= _BV(PC6);
			PORTC &= ~_BV(PC6);
#endif

			spi_send(0x00); // motion, not used
			spi_send(0x00); // observation, not used
			_x.lo = spi_recv();
			_x.hi = spi_recv();
			_y.lo = spi_recv();
			_y.hi = spi_recv();
			prev_squal = spi_recv();
			SS_HIGH;

#ifdef DEBUG_PINS
			PORTC |= _BV(PC6);
			PORTC &= ~_BV(PC6);
#endif

			if (overwrite_delta) {
				_x.all = out_dx;
				_y.all = out_dy;
			}

			int16_t cpi;
			bool as;
			int8_t lod;
			mouse_get_params(&cpi, &as, &lod);
			pmw3366_set_cpi(cpi);
			pmw3366_set_mode(as, lod);
			const uint8_t btn_dbncd = b1 | (b2 << 1);

			if ((btn_dbncd != btn_prev) || _x.all || _y.all) {
				UENUM = MOUSE_ENDPOINT;
				if (UESTA0X & (1<<NBUSYBK0)) { // untransmitted data still in bank
					UEINTX |= (1<<RXOUTI); // kill bank; RXOUTI == KILLBK
					while (UEINTX & (1<<RXOUTI));
				} else {
					// transmission's finished, or the data that should be in the
					// bank is exactly the same as what was previously transmitted
					// so that there was nothing worth transmitting before.
					btn_usb_prev = btn_usb;
					btn_usb = 0x00;
					x.all = 0;
					y.all = 0;
				}
				btn_usb |= btn_dbncd;
				x.all += _x.all;
				y.all += _y.all;
				// only transmit if there's something worth transmitting
				if ((btn_usb != btn_usb_prev)|| x.all || y.all) {
#ifdef DEBUG_PINS
					PORTC |= _BV(PC6);
					PORTC &= ~_BV(PC6);
#endif
					UEDATX = btn_usb;
					UEDATX = x.lo;
					UEDATX = x.hi;
					UEDATX = y.lo;
					UEDATX = y.hi;
					UEDATX = 0;
					UEINTX = 0x3a;
				}
			}

			btn_prev = btn_dbncd;
			++time_ticks;

			SREG = intr_state;
		}
	}
}

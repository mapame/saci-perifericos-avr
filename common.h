#define SET_BIT(reg, bit) reg |= (1 << (bit))
#define CLEAR_BIT(reg, bit) reg &= ~(1 << (bit))

//#define GET_PIN_(reg, p, bi) (reg##p) & (1 << (bi))
#define GET_PIN_(reg, p, bi) (((reg##p) >> bi) & 1)
#define SET_PIN_(reg, p, bi) (reg##p) |= (1 << (bi))
#define CLR_PIN_(reg, p, bi) (reg##p) &= ~(1 << (bi))
#define TGL_PIN_(reg, p, bi) (reg##p) ^= (1 << (bi))
#define GET_PIN(reg, pindef) GET_PIN_(reg, pindef)
#define SET_PIN(reg, pindef) SET_PIN_(reg, pindef)
#define CLR_PIN(reg, pindef) CLR_PIN_(reg, pindef)
#define TGL_PIN(reg, pindef) TGL_PIN_(reg, pindef)

#ifdef LED_ON_D3
#define PIN_LED D, 3
#else
#define PIN_LED D, 4
#endif

#define PIN_DE D, 2

#define PIN_ADDR0 B, 1
#define PIN_ADDR1 B, 0
#define PIN_ADDR2 D, 7
#define PIN_ADDR3 D, 6
#define PIN_ADDR4 D, 5

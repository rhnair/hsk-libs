/** \file
 * HSK Analog Digital Conversion implementation
 *
 * This file implements the functions defined in hsk_adc.h.
 *
 * To be able to use all 8 channels the ADC is kept in sequential mode.
 *
 * In order to reduce processing time this library uses the convention that
 * all functions terminate with ADC register page 6. Page 6 contains the
 * ADC queue request and status registers.
 *
 * @author kami
 */

#include <Infineon/XC878.h>

#include "hsk_adc.h"

#include <string.h> /* memset() */

#include "../hsk_isr/hsk_isr.h"

/**
 * Conversion clock prescaler setting for 12MHz.
 */
#define ADC_CLK_12MHz          0

/**
 * Conversion clock prescaler setting for 8MHz.
 */
#define ADC_CLK_8MHz           1

/**
 * Conversion clock prescaler setting for 6MHz.
 */
#define ADC_CLK_6MHz           2

/**
 * Conversion clock prescaler setting for 750kHz.
 */
#define ADC_CLK_750kHz         3


/**
 * Number of availbale ADC channels.
 */
#define ADC_CHANNELS           8

/**
 * Number of queue slots.
 */
#define ADC_QUEUE              4

/**
 * Holds the channel of the next conversion that will be requested.
 */
static hsk_adc_channel pdata nextChannel = ADC_CHANNELS;

/** \var targets
 * An array of target addresses to write conversion results into.
 */
static volatile union {
	/**
	 * Pointer type used for 10 bit conversions.
	 */
	uword * ptr10;

	/**
	 * Pointer type used for 8 bit conversions.
	 */
	ubyte * ptr8;
} pdata targets[ADC_CHANNELS];

/**
 * ADC_RESRxL Channel Number bits.
 */
#define BIT_CHNR               0

/**
 * CHNR bit count.
 */
#define CNT_CHNR               3

/**
 * ADC_RESRxLH Conversion Result bits.
 */
#define BIT_RESULT             6

/**
 * RESULT bit count.
 */
#define CNT_RESULT             10

/**
 * ADC_GLOBCTR Data Width bit.
 */
#define BIT_DW                 6

#pragma save
#ifdef SDCC
#pragma nooverlay
#endif
/**
 * Write the 10bit conversion result to the targeted memory address.
 *
 * @private
 */
void hsk_adc_isr10(void) using 1 {
	hsk_adc_channel idata channel;
	uword idata result;

	/* Read the result. */
	SFR_PAGE(_ad2, SST1);
	channel = (ADC_RESR0L >> BIT_CHNR) & ((1 << CNT_CHNR) - 1);
	result = (ADC_RESR0LH >> BIT_RESULT) & ((1 << CNT_RESULT) - 1);
	SFR_PAGE(_ad2, RST1);

	/* Deliver result to the target address. */
	if (targets[channel].ptr10) {
		/* Get the result bits and deliver them. */
		*targets[channel].ptr10 = result;
	}
}

/**
 * Write the 8bit conversion result to the targeted memory address.
 *
 * @private
 */
void hsk_adc_isr8(void) using 1 {
	hsk_adc_channel idata channel;
	ubyte idata result;

	/* Read the result. */
	SFR_PAGE(_ad2, SST1);
	channel = (ADC_RESR0L >> BIT_CHNR) & ((1 << CNT_CHNR) - 1);
	result = ADC_RESR0H;
	SFR_PAGE(_ad2, RST1);

	/* Deliver result to the target address. */
	if (targets[channel].ptr8) {
		/* Get the result bits and deliver them. */
		*targets[channel].ptr8 = result;
	}
}
#pragma restore

/**
 * ADC_GLOBCTR Conversion Time Control bits.
 */
#define BIT_CTC                4

/**
 * CTC bit count.
 */
#define CNT_CTC                2

/**
 * ADC_PRAR Arbitration Slot Sequential Enable bit.
 */
#define BIT_ASEN_SEQUENTIAL    6

/**
 * ADC_PRAR Arbitration Slot Parallel Enable bit.
 */
#define BIT_ASEN_PARALLEL      7

/**
 * RCRx Interrupt Enable bit.
 */
#define BIT_IEN                4

/**
 * RCRx Wait-for-Read Mode.
 */
#define BIT_WFR                6

/**
 * RCRx Valid Flag Control bit.
 */
#define BIT_VFCTR              7

/**
 * QMR0 Enable Gate bit.
 */
#define BIT_ENGT               0

/**
 * ADC_GLOBCTR Analog Part Switched On bit.
 */
#define BIT_ANON               7

/**
 * SYSCON0 Interrupt Structure 2 Mode Select bit.
 */
#define BIT_IMODE              4

void hsk_adc_init(ubyte resolution, uword __xdata convTime) {
	/* The Conversion Time Control bits, any of ADC_CLK_*. */
	ubyte ctc;
	/* The Sample Time Control bits, values from 0 to 255. */
	uword stc;

	/* Make sure the conversion target list is clean. */
	memset(targets, 0, sizeof(targets));

	/* Set ADC resolution */
	SFR_PAGE(_ad0, noSST);
	ADC_GLOBCTR = ADC_GLOBCTR & ~(1 << BIT_DW) | (resolution << BIT_DW);
	resolution = resolution == ADC_RESOLUTION_10 ? 10 : 8;

	/*
	 * Calculate conversion time parameters.
	 */
	/* Convert convTime into clock ticks. */
	convTime *= 24;
	/*
	 * Find the fastest possible CTC prescaler, based on the maximum
	 * STC value.
	 * Then find the appropriate STC value, check the Conversion Timing
	 * section of the Analog-to-Digital Converter chapter.
	 */
	if (convTime <= 1 + 2 * (258 + resolution)) {
		ctc = ADC_CLK_12MHz;
		stc = (convTime - 1) / 2 - 3 - resolution;
	} else if (convTime <= 1 + 3 * (258 + resolution)) {
		ctc = ADC_CLK_8MHz;
		stc = (convTime - 1) / 3 - 3 - resolution;
	} else if (convTime <= 1 + 4 * (258 + resolution)) {
		ctc = ADC_CLK_6MHz;
		stc = (convTime - 1) / 4 - 3 - resolution;
	} else {
		ctc = ADC_CLK_750kHz;
		stc = (convTime - 1) / 32 - 3 - resolution;
	}
	/* Make sure STC fits into an 8 bit register. */
	stc = stc >= 1 << 8 ? (1 << 8) - 1 : stc;

	/* Set ADC module clk */
	ADC_GLOBCTR = ADC_GLOBCTR & ~(((1 << CNT_CTC) - 1) << BIT_CTC) | (ctc << BIT_CTC);
	/* Set sample time in multiples of ctc scaled clock cycles. */
	ADC_INPCR0 = stc;

	/* No boundary checks. */
	ADC_LCBR = 0x00;

	/* Allow sequential arbitration mode only. */
	ADC_PRAR |= 1 << BIT_ASEN_SEQUENTIAL;
	ADC_PRAR &= ~(1 << BIT_ASEN_PARALLEL);

	/* Reset valid flag on result register 0 access. */
	SFR_PAGE(_ad4, noSST);
	ADC_RCR0 |= (1 << BIT_IEN) | (1 << BIT_WFR) | (1 << BIT_VFCTR);

	/* Use ADCSR0 interrupt. */
	SFR_PAGE(_ad5, noSST);
	ADC_CHINPR = 0x00;
	ADC_EVINPR = 0x00;

	/* Enable the queue mode gate. */
	SFR_PAGE(_ad6, noSST);
	ADC_QMR0 |= 1 << BIT_ENGT;

	/* Turn on analogue part. */
	SFR_PAGE(_ad0, noSST);
	ADC_GLOBCTR |= 1 << BIT_ANON;
	/* Wait 100ns, that's less than 3 cycles, don't bother. */

	/* Register interrupt handler. */
	EADC = 0;
	switch (resolution) {
	case 10:
		hsk_isr6.ADCSR0 = &hsk_adc_isr10;
		break;
	case 8:
		hsk_isr6.ADCSR0 = &hsk_adc_isr8;
		break;
	}
	/* Set IMODE, 1 so that EADC can be used to mask interrupts without
	 * loosing them. */
	SYSCON0 |= 1 << BIT_IMODE;
	/* Enable interrupt. */
	EADC = 1;

	SFR_PAGE(_ad6, noSST);
}

/**
 * PMCON1 ADC Disable Request bit.
 */
#define BIT_ADC_DIS            0

void hsk_adc_enable(void) {
	/* Enable clock. */
	SFR_PAGE(_su1, noSST);
	PMCON1 &= ~(1 << BIT_ADC_DIS);
	SFR_PAGE(_su0, noSST);
}

void hsk_adc_disable(void) {
	/* Stop clock in module. */
	SFR_PAGE(_su1, noSST);
	PMCON1 |= 1 << BIT_ADC_DIS;
	SFR_PAGE(_su0, noSST);
}

/**
 * QSR0 bits Filling Level.
 */
#define BIT_FILL               0

/**
 * Filling Level bit count.
 */
#define CNT_FILL               2

/**
 * QSR0 bit Queue Empty.
 */
#define BIT_EMPTY              5

void hsk_adc_open10(const hsk_adc_channel channel,
		uword * const target) {
	bool eadc = EADC;

	/* This function only works in 10 bit mode. */
	SFR_PAGE(_ad0, noSST);
	if (((ADC_GLOBCTR >> BIT_DW) & 1) != ADC_RESOLUTION_10) {
		/* This should never happen! */
		SFR_PAGE(_ad6, noSST);
		return;
	}
	SFR_PAGE(_ad6, noSST);

	EADC = 0;
	/* Register callback function. */
	targets[channel].ptr10 = target;
	EADC = eadc;

	/* Check if there are no open channels. */
	if (nextChannel >= ADC_CHANNELS) {
		/* Claim the spot as the first open channel. */
		nextChannel = channel;
	}
}

void hsk_adc_open8(const hsk_adc_channel channel,
		ubyte * const target) {
	bool eadc = EADC;

	/* This function only works in 8 bit mode. */
	SFR_PAGE(_ad0, noSST);
	if (((ADC_GLOBCTR >> BIT_DW) & 1) != ADC_RESOLUTION_8) {
		/* This should never happen! */
		SFR_PAGE(_ad6, noSST);
		return;
	}
	SFR_PAGE(_ad6, noSST);

	EADC = 0;
	/* Register callback function. */
	targets[channel].ptr8 = target;
	EADC = eadc;

	/* Check if there are no open channels. */
	if (nextChannel >= ADC_CHANNELS) {
		/* Claim the spot as the first open channel. */
		nextChannel = channel;
	}
}

void hsk_adc_close(const hsk_adc_channel channel) {
	bool eadc = EADC;
	EADC = 0;
	/* Unregister conversion target address. */
	targets[channel].ptr10 = 0;
	EADC = eadc;
	/* If this channel is scheduled for the next conversion, find an
	 * alternative. */
	if (nextChannel == channel) {
		/* Get next channel. */
		for (; nextChannel < channel + ADC_CHANNELS && !targets[nextChannel].ptr10; nextChannel++);
		nextChannel %= ADC_CHANNELS;
		/* Check whether no active channel was found. */
		if (!targets[nextChannel].ptr10) {
			nextChannel = ADC_CHANNELS;
		}
	}
}

/**
 * ADC_QINR0 Request Channel Number bits.
 */
#define BIT_REQCHNR            0

/**
 * REQCHNR bit count.
 */
#define CNT_REQCHNR            3

bool hsk_adc_service(void) {
	/* Check for available channels. */
	if (nextChannel >= ADC_CHANNELS) {
		return 0;
	}
	/* Check for a full queue. */
	if (hsk_adc_request(nextChannel)) {
		/* Find next conversion channel. */
		while (!targets[++nextChannel % ADC_CHANNELS].ptr10);
		nextChannel %= ADC_CHANNELS;
		return 1;
	}
	return 0;
}

bool hsk_adc_request(const hsk_adc_channel channel) {
	/* Check for a full queue. */
	if ((ADC_QSR0 & ((((1 << CNT_FILL) - 1) << BIT_FILL) | (1 << BIT_EMPTY))) == ((ADC_QUEUE - 1) << BIT_FILL)) {
		return 0;
	}
	/* Set next channel. */
	ADC_QINR0 = channel << BIT_REQCHNR;
	return 1;
}

#pragma save
#ifdef SDCC
#pragma nooverlay
#endif
/**
 * Special ISR for warming up 10 bit conversions.
 *
 * This is used as the ISR by hsk_adc_warmup() after the warmup countdowns
 * have been initialized. After all warmup countdowns have returned to zero
 * The original ISR will be put back in control.
 *
 * @private
 */
void hsk_adc_isr_warmup10(void) using 1 {
	ubyte idata i;

	/* Let the original ISR do its thing. */
	hsk_adc_isr10();

	/* Check whether all channels have completed warmup. */
	for (i = 0; i < ADC_CHANNELS; i++) {
		if (targets[i].ptr10 && *targets[i].ptr10 == -1) {
			/* Bail out if a channel is not warmed up. */
			return;
		}
	}

	/* Hand over to the original ISR. */
	hsk_isr6.ADCSR0 = &hsk_adc_isr10;
}
#pragma restore

void hsk_adc_warmup10(void) {
	ubyte i;

	/* This function only works in 10 bit mode. */
	SFR_PAGE(_ad0, noSST);
	if (((ADC_GLOBCTR >> BIT_DW) & 1) != ADC_RESOLUTION_10) {
		/* This should never happen! */
		SFR_PAGE(_ad6, noSST);
		return;
	}
	SFR_PAGE(_ad6, noSST);

	/* Set all conversion targets to an invalid value so the value that
	 * was written can be detected. */
	for (i = 0; i < ADC_CHANNELS; i++) {
		if (targets[i].ptr10) {
			*targets[i].ptr10 = 0xffff;
		}
	}

	/* Hijack the ISR. */
	EADC = 0;
	hsk_isr6.ADCSR0 = &hsk_adc_isr_warmup10;
	EADC = 1;

	/*
	 * Now just keep on performing conversion until the warumup isr
	 * unregisters itself.
	 */
	while (hsk_isr6.ADCSR0 == &hsk_adc_isr_warmup10) {
		hsk_adc_service();
	}
}


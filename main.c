#pragma clang diagnostic push
#pragma ide diagnostic ignored "cppcoreguidelines-narrowing-conversions"
#pragma ide diagnostic ignored "EndlessLoop"
#pragma ide diagnostic ignored "hicpp-signed-bitwise"

#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "usbdrv/oddebug.h"
#include "usbdrv/usbdrv.h"
#include "main.h"

static report_t reportBuffer;
static uint8_t idleRate;
static uint8_t enabled = 1;

usbMsgLen_t usbFunctionSetup(uchar data[8]) {
    usbRequest_t *rq = (void *) data;
    if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {
        if (rq->bRequest == USBRQ_HID_GET_REPORT) {
            usbMsgPtr = (void *) &reportBuffer;
            return sizeof(reportBuffer);
        } else if (rq->bRequest == USBRQ_HID_GET_IDLE) {
            usbMsgPtr = &idleRate;
            return 1;
        } else if (rq->bRequest == USBRQ_HID_SET_IDLE) {
            idleRate = rq->wValue.bytes[1];
        }
    }

    return 0;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-infinite-loop"
#pragma ide diagnostic ignored "bugprone-incorrect-roundings"

/* Oscillator calibration routine borrowed from here:
 *
 * https://www.obdev.at/products/vusb/easylogger.html
 *
 * Calibrate the RC oscillator to 8.25 MHz. The core clock of 16.5 MHz is
 * derived from the 66 MHz peripheral clock by dividing. Our timing reference
 * is the Start Of Frame signal (a single SE0 bit) available immediately after
 * a USB RESET. We first do a binary search for the OSCCAL value and then
 * optimize this value with a neighboorhod search.
 * This algorithm may also be used to calibrate the RC oscillator directly to
 * 12 MHz (no PLL involved, can therefore be used on almost ALL AVRs), but this
 * is wide outside the spec for the OSCCAL value and the required precision for
 * the 12 MHz clock! Use the RC oscillator calibrated to 12 MHz for
 * experimental purposes only!
 */
static void calibrateOscillator(void) {
    uchar step = 128;
    uchar trialValue = 0, optimumValue;
    int x, optimumDev,
            targetValue = (unsigned) (1499 * (double) F_CPU / 10.5e6 + 0.5);

    /* do a binary search: */
    do {
        OSCCAL = trialValue + step;
        x = usbMeasureFrameLength(); /* proportional to current real frequency */
        if (x < targetValue)         /* frequency still too low */
            trialValue += step;
        step >>= 1;
    } while (step > 0);

    /* We have a precision of +/- 1 for optimum OSCCAL here */
    /* now do a neighborhood search for optimum value */
    optimumValue = trialValue;
    optimumDev = x; /* this is certainly far away from optimum */
    for (OSCCAL = trialValue - 1; OSCCAL <= trialValue + 1; OSCCAL++) {
        x = usbMeasureFrameLength() - targetValue;
        if (x < 0)
            x = -x;
        if (x < optimumDev) {
            optimumDev = x;
            optimumValue = OSCCAL;
        }
    }
    OSCCAL = optimumValue;
}

#pragma clang diagnostic pop

void usbEventResetReady(void) {
    cli();
    calibrateOscillator();
    sei();
    eeprom_write_byte(0, OSCCAL);
}

void keyPoll(void) {
    static uint8_t key_prev;
    uint8_t key;

    key = KEY_PINS & (1 << KEY_BIT);
    if (key_prev != key) {
        key_prev = key;
        if (!key) {
            enabled = !enabled;
        }
    }
}

int main(void) {
    uint8_t storedOscCal;
    uint8_t mode = 0;
    uint8_t status = 0;

    KEY_PORT |= 1 << KEY_BIT;

    storedOscCal = eeprom_read_byte(0);
    if (storedOscCal != 0xff) {
        OSCCAL = storedOscCal;
    }

    usbDeviceDisconnect();
    for (uint8_t i = 0; i < 20; i++) {
        _delay_ms(15);
    }
    usbDeviceConnect();

    wdt_enable(WDTO_1S);
    usbInit();
    sei();

    for (;;) {
        wdt_reset();
        keyPoll();
        usbPoll();

        if (enabled && usbInterruptIsReady()) {

            switch (mode) {

                default:
                    mode = 0;
                    reportBuffer.dx = 1;
                    reportBuffer.dy = 0;
                    break;

                case 1:
                    reportBuffer.dx = 0;
                    reportBuffer.dy = 1;
                    break;

                case 2:
                    reportBuffer.dx = -1;
                    reportBuffer.dy = 0;
                    break;

                case 3:
                    reportBuffer.dx = 0;
                    reportBuffer.dy = -1;
                    break;
            }

            if (status >= SIDE_LEN) {
                status = 0;
                mode++;
            } else {
                status++;
            }

            usbSetInterrupt((void *) &reportBuffer, sizeof(reportBuffer));
        }
    }

    return 0;
}

#pragma clang diagnostic pop
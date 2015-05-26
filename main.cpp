// Copyright (c) 2015 Michael Smith, All Rights Reserved
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//  o Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  o Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <sysctl.h>
#include <pin.h>
#include <timer.h>
#include <uart.h>
#include <interrupt.h>

#define SWITCH      P0_2
#define RELAY       P0_3

#define flashTimer  Timer0
#define switchTimer Timer1

#define DEBUG(str)  do { UART0 << str << "\r\n"; } while(0)

static const unsigned   kDebounceTime = 10 * 24000;
static const unsigned   kMinOnTime = 200 * 24000;
static const unsigned   kMinOffTime = 200 * 24000;
static const unsigned   kSwitchTime = 350 * 24000;

extern "C" int main();

int
main()
{
    enum {
        WAITING_OFF,
        FLASH_DEBOUNCE,
        FLASHING,
        FLASH_HOLD,
        FLASH_WAIT,
        LATCHED_ON,
        CANCEL_DEBOUNCE,
        CANCELLING
    } state = WAITING_OFF;

    Sysctl::init_24MHz();

    // UART output for debugging
    UART0_TXD.claim_pin(P0_4);
    UART0.configure(57600);
    DEBUG("START");

    // relay output configured and driven low
    RELAY.configure(Pin::Output, Pin::PushPull);
    RELAY << 0;

    // switch input configured and waiting to be pulled low
    SWITCH.configure(Pin::Input, Pin::PullUp | Pin::Invert);

    // spin forever
    for (;;) {
        //Interrupt::wait();

        switch (state) {
        case WAITING_OFF:

            // reset all our state
            RELAY << 0;
            switchTimer.cancel();
            flashTimer.cancel();

            if (SWITCH) {                           // switch closed, start debouncing
                switchTimer.configure(kDebounceTime);
                state = FLASH_DEBOUNCE;
                DEBUG("FLASH_DEBOUNCE");
            }
            break;

        case FLASH_DEBOUNCE:
            if (!SWITCH) {                          // switch opened before debounce period expired
                state = WAITING_OFF;
                DEBUG("WAITING_OFF1");
            } else if (switchTimer.expired()) {     // debounce period expired, switch is really closed
                state = FLASHING;
                RELAY << 1;
                switchTimer.configure(kSwitchTime);
                flashTimer.configure(kMinOnTime);
                DEBUG("FLASHING");
            }
            break;

        case FLASHING:
            if (!SWITCH) {                          // switch has opened
                if (switchTimer.expired()) {        // latch-on timer has expired, stay on until cancelled
                    state = LATCHED_ON;
                    DEBUG("LATCHED_ON1");
                } else {
                    state = FLASH_HOLD;
                    DEBUG("FLASH_HOLD");
                }
            }
            break;

        case FLASH_HOLD:
            if (flashTimer.expired()) {             // wait until the minimum flash time has passed
                RELAY << 0;
                flashTimer.configure(kMinOffTime);
                state = FLASH_WAIT;
                DEBUG("FLASH_WAIT");
            }
            break;

        case FLASH_WAIT:
            if (flashTimer.expired()) {
                state = WAITING_OFF;
                DEBUG("WAITING_OFF2");
            }
            break;

        case LATCHED_ON:
            if (SWITCH) {                           // switch closed, start debouncing
                switchTimer.configure(kDebounceTime);
                state = CANCEL_DEBOUNCE;
                DEBUG("CANCEL_DEBOUNCE");
            }
            break;

        case CANCEL_DEBOUNCE:
            if (!SWITCH) {                          // switch opened before debounce period expired
                state = LATCHED_ON;
                DEBUG("LATCHED_ON2");
            } else if (switchTimer.expired()) {
                state = CANCELLING;
                DEBUG("CANCELLING");
            }
            break;

        case CANCELLING:
            if (!SWITCH) {
                state = WAITING_OFF;
                DEBUG("WAITING_OFF3");
            }
            break;

        default:
            switchTimer.cancel();
            state = WAITING_OFF;
            DEBUG("WAITING_OFF ERROR");
        };
    }
}

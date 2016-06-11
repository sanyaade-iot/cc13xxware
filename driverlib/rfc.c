/******************************************************************************
*  Filename:       rfc.c
*  Revised:        2016-04-07 15:04:05 +0200 (Thu, 07 Apr 2016)
*  Revision:       46052
*
*  Description:    Driver for the RF Core.
*
*  Copyright (c) 2015, Texas Instruments Incorporated
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*
*  1) Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
*
*  2) Redistributions in binary form must reproduce the above copyright notice,
*     this list of conditions and the following disclaimer in the documentation
*     and/or other materials provided with the distribution.
*
*  3) Neither the name of the ORGANIZATION nor the names of its contributors may
*     be used to endorse or promote products derived from this software without
*     specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
*  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
*  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
*  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
*  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
*  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
*  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
*  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
*  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
*  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*
******************************************************************************/

#include <driverlib/rfc.h>

#define RFC_RESERVED0               0x40044108
#define RFC_RESERVED1               0x40044114
#define RFC_RESERVED2               0x4004410C
#define RFC_RESERVED3               0x40044100

// Position of divider value
#define CONFIG_MISC_ADC_DIVIDER             27
#define CONFIG_MISC_ADC_DIVIDER_BM  0xF8000000U

// RTrim positions
#define FCFG1_O_CONFIG_MISC_ADC_PO_TAIL_RES_TRIM_M 0x003C0000
#define FCFG1_O_CONFIG_MISC_ADC_PO_TAIL_RES_TRIM_S 18
#define FCFG1_O_CONFIG_MISC_ADC_DIV6_PO_TAIL_RES_TRIM_M 0x003C0000
#define FCFG1_O_CONFIG_MISC_ADC_DIV6_PO_TAIL_RES_TRIM_S 18
#define FCFG1_O_CONFIG_MISC_ADC_DIV10_PO_TAIL_RES_TRIM_M 0x003C0000
#define FCFG1_O_CONFIG_MISC_ADC_DIV10_PO_TAIL_RES_TRIM_S 18
#define FCFG1_O_CONFIG_MISC_ADC_DIV12_PO_TAIL_RES_TRIM_M 0x003C0000
#define FCFG1_O_CONFIG_MISC_ADC_DIV12_PO_TAIL_RES_TRIM_S 18
#define FCFG1_O_CONFIG_MISC_ADC_DIV15_PO_TAIL_RES_TRIM_M 0x003C0000
#define FCFG1_O_CONFIG_MISC_ADC_DIV15_PO_TAIL_RES_TRIM_S 18
#define FCFG1_O_CONFIG_MISC_ADC_DIV30_PO_TAIL_RES_TRIM_M 0x003C0000
#define FCFG1_O_CONFIG_MISC_ADC_DIV30_PO_TAIL_RES_TRIM_S 18

//*****************************************************************************
//
// Get and clear CPE interrupt flags
//
//*****************************************************************************
uint32_t
RFCCpeIntGetAndClear(void)
{
    uint32_t ui32Ifg = HWREG(RFC_DBELL_BASE+RFC_DBELL_O_RFCPEIFG);

    do {
        HWREG(RFC_DBELL_BASE+RFC_DBELL_O_RFCPEIFG) = ~ui32Ifg;
    } while (HWREG(RFC_DBELL_BASE+RFC_DBELL_O_RFCPEIFG) & ui32Ifg);

    return (ui32Ifg);
}


//*****************************************************************************
//
// Send command to doorbell and wait for ack
//
//*****************************************************************************
uint32_t
RFCDoorbellSendTo(uint32_t pOp)
{
    while(HWREG(RFC_DBELL_BASE + RFC_DBELL_O_CMDR) != 0);

    RFCAckIntClear();

    HWREG(RFC_DBELL_BASE+RFC_DBELL_O_CMDR) = pOp;

    while(!HWREG(RFC_DBELL_BASE + RFC_DBELL_O_RFACKIFG));
    RFCAckIntClear();

    return(HWREG(RFC_DBELL_BASE + RFC_DBELL_O_CMDSTA));
}


//*****************************************************************************
//
// Turn off synth, NOTE: Radio will no longer respond to commands!
//
//*****************************************************************************
void
RFCSynthPowerDown()
{
  // Disable CPE clock, enable FSCA clock. NOTE: Radio will no longer respond to commands!
  HWREG(RFC_PWR_NONBUF_BASE + RFC_PWR_O_PWMCLKEN) = (HWREG(RFC_PWR_NONBUF_BASE + RFC_PWR_O_PWMCLKEN) & ~RFC_PWR_PWMCLKEN_CPE_M) | RFC_PWR_PWMCLKEN_FSCA_M;

  (*((volatile unsigned long *)(RFC_RESERVED0))) = 3;
  (*((volatile unsigned long *)(RFC_RESERVED1))) = 0x1030;
  (*((volatile unsigned long *)(RFC_RESERVED2))) = 1;
  (*((volatile unsigned long *)(RFC_RESERVED1))) = 0x50;
  (*((volatile unsigned long *)(RFC_RESERVED2))) = 1;
  (*((volatile unsigned long *)(RFC_RESERVED1))) = 0x650;
  (*((volatile unsigned long *)(RFC_RESERVED2))) = 1;
  (*((volatile unsigned long *)(RFC_RESERVED3))) = 1;
}


//*****************************************************************************
//
// Read RF Trim from flash using the CM3
//
//*****************************************************************************
void RFCRfTrimRead(rfc_radioOp_t *pOpSetup, rfTrim_t* pRfTrim)
{
    int divider;
    // Check which setup command is used
    switch (pOpSetup->commandNo)
    {
    case CMD_RADIO_SETUP:
        divider = ((rfc_CMD_RADIO_SETUP_t *)pOpSetup)->loDivider;
        break;
    case CMD_PROP_RADIO_DIV_SETUP:
        divider = ((rfc_CMD_PROP_RADIO_DIV_SETUP_t *)pOpSetup)->loDivider;
        break;
    default:
        divider = 0; // Use 2.4 GHz
        break;
    }

    // Read trim from FCFG1
    pRfTrim->configIfAdc = HWREG(FCFG1_BASE + FCFG1_O_CONFIG_IF_ADC);
    switch (divider)
    {
    case 5:
        pRfTrim->configRfFrontend = HWREG(FCFG1_BASE + FCFG1_O_CONFIG_RF_FRONTEND_DIV5);
        pRfTrim->configSynth = HWREG(FCFG1_BASE + FCFG1_O_CONFIG_SYNTH_DIV5);
        pRfTrim->configMiscAdc = (HWREG(FCFG1_BASE + FCFG1_O_CONFIG_MISC_ADC_DIV5)
                               & ~CONFIG_MISC_ADC_DIVIDER_BM) | (5U << CONFIG_MISC_ADC_DIVIDER);
        break;

    case 6:
        pRfTrim->configRfFrontend = HWREG(FCFG1_BASE + FCFG1_O_CONFIG_RF_FRONTEND_DIV6);
        pRfTrim->configSynth = HWREG(FCFG1_BASE + FCFG1_O_CONFIG_SYNTH_DIV6);
        pRfTrim->configMiscAdc = (HWREG(FCFG1_BASE + FCFG1_O_CONFIG_MISC_ADC_DIV6)
                               & ~CONFIG_MISC_ADC_DIVIDER_BM) | (6U << CONFIG_MISC_ADC_DIVIDER);
        break;

    case 10:
        pRfTrim->configRfFrontend = HWREG(FCFG1_BASE + FCFG1_O_CONFIG_RF_FRONTEND_DIV10);
        pRfTrim->configSynth = HWREG(FCFG1_BASE + FCFG1_O_CONFIG_SYNTH_DIV10);
        pRfTrim->configMiscAdc = (HWREG(FCFG1_BASE + FCFG1_O_CONFIG_MISC_ADC_DIV10)
                               & ~CONFIG_MISC_ADC_DIVIDER_BM) | (10U << CONFIG_MISC_ADC_DIVIDER);
        break;

    case 12:
        pRfTrim->configRfFrontend = HWREG(FCFG1_BASE + FCFG1_O_CONFIG_RF_FRONTEND_DIV12);
        pRfTrim->configSynth = HWREG(FCFG1_BASE + FCFG1_O_CONFIG_SYNTH_DIV12);
        pRfTrim->configMiscAdc = (HWREG(FCFG1_BASE + FCFG1_O_CONFIG_MISC_ADC_DIV12)
                               & ~CONFIG_MISC_ADC_DIVIDER_BM) | (12U << CONFIG_MISC_ADC_DIVIDER);
        break;

    case 15:
        pRfTrim->configRfFrontend = HWREG(FCFG1_BASE + FCFG1_O_CONFIG_RF_FRONTEND_DIV15);
        pRfTrim->configSynth = HWREG(FCFG1_BASE + FCFG1_O_CONFIG_SYNTH_DIV15);
        pRfTrim->configMiscAdc = (HWREG(FCFG1_BASE + FCFG1_O_CONFIG_MISC_ADC_DIV15)
                               & ~CONFIG_MISC_ADC_DIVIDER_BM) | (15U << CONFIG_MISC_ADC_DIVIDER);
        break;

    case 30:
        pRfTrim->configRfFrontend = HWREG(FCFG1_BASE + FCFG1_O_CONFIG_RF_FRONTEND_DIV30);
        pRfTrim->configSynth = HWREG(FCFG1_BASE + FCFG1_O_CONFIG_SYNTH_DIV30);
        pRfTrim->configMiscAdc = (HWREG(FCFG1_BASE + FCFG1_O_CONFIG_MISC_ADC_DIV30)
                               & ~CONFIG_MISC_ADC_DIVIDER_BM) | (30U << CONFIG_MISC_ADC_DIVIDER);
        break;

    default:
        pRfTrim->configRfFrontend = HWREG(FCFG1_BASE + FCFG1_O_CONFIG_RF_FRONTEND);
        pRfTrim->configSynth = HWREG(FCFG1_BASE + FCFG1_O_CONFIG_SYNTH);
        // Make sure configMiscAdc is not 0 by setting an unused bit to 1
        pRfTrim->configMiscAdc = (HWREG(FCFG1_BASE + FCFG1_O_CONFIG_MISC_ADC)
                               & ~CONFIG_MISC_ADC_DIVIDER_BM) | (2U << CONFIG_MISC_ADC_DIVIDER);
        break;
   }
}


//*****************************************************************************
//
// Check Override RTrim vs FCFG RTrim
//
//*****************************************************************************
void RFCRTrim(rfc_radioOp_t *pOpSetup)
{
    int32_t divider;
    uint32_t fcfg1_rtrim;
    uint32_t *pOverride;
    int32_t override_index;
    uint32_t override_value;
    uint32_t override_rtrim = 0;

    // Check which setup command is used
    switch (pOpSetup->commandNo)
    {
    case CMD_RADIO_SETUP:
        divider = ((rfc_CMD_RADIO_SETUP_t *)pOpSetup)->loDivider;
        pOverride = ((rfc_CMD_RADIO_SETUP_t *)pOpSetup)->pRegOverride;
        break;
    case CMD_PROP_RADIO_SETUP:
        divider = 2;
        pOverride = ((rfc_CMD_PROP_RADIO_SETUP_t *)pOpSetup)->pRegOverride;
        break;
    case CMD_PROP_RADIO_DIV_SETUP:
        divider = ((rfc_CMD_PROP_RADIO_DIV_SETUP_t *)pOpSetup)->loDivider;
        pOverride = ((rfc_CMD_PROP_RADIO_DIV_SETUP_t *)pOpSetup)->pRegOverride;
        break;
    default:
        return;
    }

    if (pOverride == 0)
    {
        //  Did not find override, return
        return;
    }


    // Search top 5 overrides for RTRIM
    for(override_index = 0; override_index < 5; override_index++)
    {
        override_value = pOverride[override_index];
        if((override_value & 0xFFFF) == 0x4038)
        {
            override_rtrim = (override_value & 0xF0000) >> 16;
            break;
        }
    }

    if (override_rtrim == 0)
    {
        //  Did not find override, return
        return;
    }

    // Read trim from FCFG1
    switch (divider)
    {
    case 2:
        fcfg1_rtrim = (HWREG(FCFG1_BASE + FCFG1_O_CONFIG_MISC_ADC)
                       & FCFG1_O_CONFIG_MISC_ADC_PO_TAIL_RES_TRIM_M) >> FCFG1_O_CONFIG_MISC_ADC_PO_TAIL_RES_TRIM_S;
        break;
    case 5:
        // Legacy
        fcfg1_rtrim = (HWREG(FCFG1_BASE + FCFG1_O_MISC_OTP_DATA)
                       & FCFG1_MISC_OTP_DATA_PO_TAIL_RES_TRIM_M) >> FCFG1_MISC_OTP_DATA_PO_TAIL_RES_TRIM_S;
        break;
    case 6:
        fcfg1_rtrim = (HWREG(FCFG1_BASE + FCFG1_O_CONFIG_MISC_ADC_DIV6)
                       & FCFG1_O_CONFIG_MISC_ADC_DIV6_PO_TAIL_RES_TRIM_M) >> FCFG1_O_CONFIG_MISC_ADC_DIV6_PO_TAIL_RES_TRIM_S;
        break;
    case 10:
        fcfg1_rtrim = (HWREG(FCFG1_BASE + FCFG1_O_CONFIG_MISC_ADC_DIV10)
                       & FCFG1_O_CONFIG_MISC_ADC_DIV10_PO_TAIL_RES_TRIM_M) >> FCFG1_O_CONFIG_MISC_ADC_DIV10_PO_TAIL_RES_TRIM_S;
        break;
    case 12:
        fcfg1_rtrim = (HWREG(FCFG1_BASE + FCFG1_O_CONFIG_MISC_ADC_DIV12)
                       & FCFG1_O_CONFIG_MISC_ADC_DIV12_PO_TAIL_RES_TRIM_M) >> FCFG1_O_CONFIG_MISC_ADC_DIV12_PO_TAIL_RES_TRIM_S;
        break;
    case 15:
        fcfg1_rtrim = (HWREG(FCFG1_BASE + FCFG1_O_CONFIG_MISC_ADC_DIV15)
                       & FCFG1_O_CONFIG_MISC_ADC_DIV15_PO_TAIL_RES_TRIM_M) >> FCFG1_O_CONFIG_MISC_ADC_DIV15_PO_TAIL_RES_TRIM_S;
        break;
    case 30:
        fcfg1_rtrim = (HWREG(FCFG1_BASE + FCFG1_O_CONFIG_MISC_ADC_DIV30)
                       & FCFG1_O_CONFIG_MISC_ADC_DIV30_PO_TAIL_RES_TRIM_M) >> FCFG1_O_CONFIG_MISC_ADC_DIV30_PO_TAIL_RES_TRIM_S;
        break;
    default:
        fcfg1_rtrim = (HWREG(FCFG1_BASE + FCFG1_O_CONFIG_MISC_ADC)
                       & FCFG1_O_CONFIG_MISC_ADC_PO_TAIL_RES_TRIM_M) >> FCFG1_O_CONFIG_MISC_ADC_PO_TAIL_RES_TRIM_S;
        break;
    }

    // Check for early samples
    if(fcfg1_rtrim == 0xF)
    {
        // set default
        switch (divider)
        {
        case 5:
        case 10:
        case 15:
        case 30:
            pOverride[override_index] = (override_value & 0xFFF0FFFF) | (0x7 << 16);
            break;
        case 2:
        case 6:
        case 12:
        default:
            pOverride[override_index] = (override_value & 0xFFF0FFFF) | (0x4 << 16);
            break;
        }
    }
    else
    {
        // Test Override vs FCFG1 limit.
        if(override_rtrim >= fcfg1_rtrim)
        {
            // Do nothing
            ;
        }
        else
        {
            // Set override to FCFG1 limit value
            pOverride[override_index] = (override_value & 0xFFF0FFFF) | (fcfg1_rtrim << 16);
        }
    }
}


//*****************************************************************************
//
// Write preloaded RF trim values to CM0
//
//*****************************************************************************
void RFCRfTrimSet(rfTrim_t* pRfTrim)
{
    memcpy((void*)&HWREG(0x21000018), (void*)pRfTrim, sizeof(rfTrim_t));
}


//*****************************************************************************
//
// Handle support for DriverLib in ROM:
// This section will undo prototype renaming made in the header file
//
//*****************************************************************************
#if !defined(DOXYGEN)
    #undef  RFCCpeIntGetAndClear
    #define RFCCpeIntGetAndClear            NOROM_RFCCpeIntGetAndClear
    #undef  RFCDoorbellSendTo
    #define RFCDoorbellSendTo               NOROM_RFCDoorbellSendTo
    #undef  RFCSynthPowerDown
    #define RFCSynthPowerDown               NOROM_RFCSynthPowerDown
    #undef  RFCRfTrimRead
    #define RFCRfTrimRead                   NOROM_RFCRfTrimRead
    #undef  RFCRfTrimSet
    #define RFCRfTrimSet                    NOROM_RFCRfTrimSet
    #undef  RFCRTrim
    #define RFCRTrim                        NOROM_RFCRTrim
#endif

// See rfc.h for implementation

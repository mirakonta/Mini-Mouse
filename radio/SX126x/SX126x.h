/*

  __  __ _       _                                 
 |  \/  (_)     (_)                                
 | \  / |_ _ __  _ _ __ ___   ___  _   _ ___  ___  
 | |\/| | | '_ \| | '_ ` _ \ / _ \| | | / __|/ _ \
 | |  | | | | | | | | | | | | (_) | |_| \__ \  __/ 
 |_|  |_|_|_| |_|_|_| |_| |_|\___/ \__,_|___/\___| 
                                                   
                                                   
Description       : LoraWan Phy Layer objets.  


License           : Revised BSD License, see LICENSE.TXT file include in the project

Maintainer        : Olivier Gimenez (SEMTECH)
*/
#ifndef SX126X_H
#define SX126X_H
#include <math.h>
#include <stdint.h>
#include "radio.h"


/*!
 * \brief Provides the frequency of the chip running on the radio and the frequency step
 * \remark These defines are used for computing the frequency divider to set the RF frequency
 */
// TODO: Move it ?
#define RADIO_XTAL_FREQ                                   ( double )32000000
#define RADIO_FREQ_DIV                                    ( double )pow( 2.0, 25.0 )
#define RADIO_FREQ_STEP                                   ( double )( RADIO_XTAL_FREQ / RADIO_FREQ_DIV )


class SX126x : public Radio {
    public:
        SX126x( );
    
        virtual void SendLora(
                        uint8_t    *payload,
                        uint8_t    payloadSize,
                        uint8_t    SF,
                        Radio::Bandwith_t BW,
                        uint32_t   channel,
                        int8_t     power
                    );
        
        virtual void Sleep( void );
    private:
        typedef enum {
            CALIBRATE_IMAGE         = 0x98,
            CLR_IRQ_STATUS          = 0x02,
            SET_DIO_IRQ_PARAMS      = 0x08,
            SET_MODULATION_PARAMS   = 0x8B,
            SET_PA_CONFIG           = 0x95,
            SET_PACKET_PARAMS       = 0x8C,
            SET_PACKET_TYPE         = 0x8A,
            SET_RF_FREQUENCY        = 0x86,
            SET_SLEEP               = 0x84,
            SET_STANDBY             = 0x80,
            SET_TX                  = 0x83,
            SET_TX_PARAMS           = 0x8E,
            SET_BUFFER_BASE_ADDRESS = 0x8F,
            WRITE_BUFFER            = 0x0E,
            WRITE_REGISTER          = 0x0D
        } OpCode_t;
        typedef enum {
            REG_LORA_SYNC_WORD_MSB  = 0x0740,
            REG_OCP                 = 0x08E7,
        } Reg_t;
        typedef enum
        {
            STDBY_RC                                = 0x00,
            STDBY_XOSC                              = 0x01,
        } RadioStandbyModes_t;

        /*!
         * \brief Calibrates the Image rejection depending of the frequency
         * @param [IN] Operating frequency
         */
        void CalibrateImage( uint32_t frequency );
        
        /*!
         * \brief Wakeup the radio if it is in Sleep mode and check that Busy is low
         */
        void CheckDeviceReady( void );
        
        /*!
         * \brief Clears the IRQs
         * @param [IN]  irq           IRQ(s) to be cleared
         */
        void ClearIrqStatus( uint16_t irq );
        
        /*!
         * \brief Sets the data buffer base address for transmission and reception
         * @param [IN]  txBaseAddress Transmission base address
         * @param [IN]  rxBaseAddress Reception base address
         */
        void SetBufferBaseAddress( uint8_t txBaseAddress, uint8_t rxBaseAddress );

        
        /*!
         * \brief   Sets the IRQ mask and DIO masks
         *
         * \param [IN]  irqMask       General IRQ mask
         * \param [IN]  dio1Mask      DIO1 mask
         * \param [IN]  dio2Mask      DIO2 mask
         * \param [IN]  dio3Mask      DIO3 mask
         */
        void SetDioIrqParams(
                        uint16_t irqMask,
                        uint16_t dio1Mask,
                        uint16_t dio2Mask,
                        uint16_t dio3Mask
                    );

        
        /*!
         * \brief Set the modulation parameters
         * @param [IN] Modulation (LoRa/FSK)
         * @param [IN] Speading factor
         * @param [IN] Bandwith
         */
        void SetModulationParams(
                    Modulation_t modulation,
                    uint8_t SF,
                    Bandwith_t BW
                );
        /*!
         * \brief Sets the packet parameters
         */
        void SetPacketParams(
                    Radio::Modulation_t modulation,
                    uint8_t payloadSize
                );
        
        /*!
         * \brief Sets the transmission parameters
         *
         * @param [in]  paDutyCycle     Duty Cycle for the PA
         * @param [in]  hpMax           0 for sx1261, 7 for sx1262
         * @param [in]  deviceSel       1 for sx1261, 0 for sx1262
         * @param [in]  paLut           0 for 14dBm LUT, 1 for 22dBm LUT
         */

        void SetPaConfig(
                    uint8_t paDutyCycle,
                    uint8_t hpMax,
                    uint8_t deviceSel,
                    uint8_t paLut
                );
        
        /*!
         * \brief Set packet type
         * @param [IN] Modulation (LoRa/FSK)
         */
        void SetPacketType( Radio::Modulation_t modulation );
        
        /*!
         * \brief Set the RF frequency
         * @param [IN] Frequency [Hz]
         */
        void SetRfFrequency( uint32_t frequency );
        
        /*!
         * \brief Sets the radio in configuration mode
         * @param [IN]  mode          Standby mode to put the radio into
         */
        void SetStandby( RadioStandbyModes_t mode );
        
        /*!
         * \brief Sets the radio in transmission mode
         * @param [IN]  timeout       Structure describing the transmission timeout value
         */
         // TODO - Use µs
        void SetTx( uint32_t timeout );
        
        /*!
         * \brief Sets the transmission parameters
         * @param [IN]  RF output power [-3..15] dBm
         * @param [IN]  Transmission ramp up time
         */
        void SetTxParams( int8_t power );
        
        
        /*!
         * \brief Blocking loop to wait while the Busy pin in high
         */
        void WaitOnBusy( void );
        
        /*!
         * \brief Write data to the buffer holding the payload in the radio
         *
         * \param [in]  offset        The offset to start writing the payload
         * \param [in]  buffer        The data to be written (the payload)
         * \param [in]  size          The number of bytes to be written
         */
        void WriteBuffer( uint8_t offset, uint8_t *buffer, uint8_t size );
        
        /*!
         * \brief Write a command to the SX126x
         * @param [IN] Operation code
         * @param [IN] In/Out buffer
         * @param [IN] Size of the buffer
         */
        void WriteCommand( OpCode_t opCode, uint8_t *buffer, uint16_t size );
        
        /*!
         * \brief Write severak bytes of data to the radio memory
         *
         * @param [in] address      The address of the first byte to write in the radio
         * @param [in] value        The data to be written in radio's memory
         * @param [in] size         Size of the data
         */
        void WriteRegisters( uint16_t address, uint8_t *buffer, uint16_t size );

};
#endif

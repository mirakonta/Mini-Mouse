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
#ifndef RADIO_H
#define RADIO_H
#include <stdint.h>
#include "mbed.h"
#include "Define.h"


class Radio {
    public:
        typedef enum {
            BW_125 = 0,
            BW_250,
            BW_500
        } Bandwith_t;
        
        typedef enum {
            FSK = 0,
            LORA,
        } Modulation_t;

        // TODO SF enum ?
        /*
        typedef enum {
            SF_12,
            SF_11,
            SF_10,
            SF_9,
            SF_8,
            SF_7,
            SF_6,
            SF_5
        } SF_t;
        */
        
        virtual ~Radio( ) { };
        //Radio( );
        
        
        /*!
         * \brief Init the radio
         */
        void Init( uint32_t loraSyncword );
        
        /*!
         * \brief Sets the radio in sleep mode
         */
        virtual void Sleep( void ) = 0;
        
        /*!
         * \brief Send a payload in LoRa modulation
         * @param [IN] Payload
         * @param [IN] Payload size
         * @param [IN] Speading factor
         * @param [IN] Bandwith
         * @param [IN] Channel [Hz]
         * @param [IN] Power [dBm]
         */
        // TODO BW type.
        virtual void SendLora(
                        uint8_t    *payload,
                        uint8_t    payloadSize,
                        uint8_t    SF,
                        Bandwith_t BW,
                        uint32_t   channel,
                        int8_t     power
                    ) = 0;
        
        /*!
         * \brief Sets the radio in transmission mode
         * @param [IN] timeout [us]
         *                     [0: continuous, others timeout] // TODO: Check continous for Tx ?
         */
//virtual void Tx( uint32_t timeout ) = 0;
        
    private:
        uint32_t LoraSyncword;
};
#endif

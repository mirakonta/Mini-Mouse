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

#include "SX126x.h"
#include <string.h>
#include "Define.h"
#include "mbed.h"

/************************************************************************************************
 *                                 Public  Methods                                              *
 ************************************************************************************************/
// TODO: Clean instantiations
SPI spi( LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK );
DigitalIn pinBusy( PB_3 );
extern DigitalOut pinCS;

SX126x::SX126x(  ) {
    // TODO
}

void SX126x::SendLora(
                        uint8_t    *payload,
                        uint8_t    payloadSize,
                        uint8_t    SF,
                        Radio::Bandwith_t BW,
                        uint32_t   channel,
                        int8_t     power
                    ) {
    uint8_t buff[2];
    // Init radio
    SetStandby( STDBY_XOSC ); // TODO, why pinCS=1 failling 1st time ?
    SetStandby( STDBY_XOSC );
                        SetStandby( STDBY_XOSC );
    // Configure the radio
    SetPacketType( Radio::LORA );
    SetRfFrequency( channel );
    SetModulationParams( Radio::LORA , /*SF*/ 7, BW );
    SetPacketParams( Radio::LORA, payloadSize );
    SetTxParams( power );
    buff[0] = 0x34;
    buff[1] = 0x44;
    WriteRegisters( REG_LORA_SYNC_WORD_MSB, buff, 2 );
    // Send the payload to the radio
    SetBufferBaseAddress( 0, 0 );
    WriteBuffer( 0, payload, payloadSize );
    // TODO - Set IRQ type
    SetDioIrqParams( 0x01, 0x01, 0x00, 0x00 );
    ClearIrqStatus( 0xFFFF );
    // Send !
    SetTx( 0 );
    wait( 10 );
}

void SX126x::Sleep( void ) {
    // TODO
}

/************************************************************************************************
 *                                Private  Methods                                              *
 ************************************************************************************************/
void SX126x::CalibrateImage( uint32_t freq )
{
    uint8_t calFreq[2];
    
    if( freq > 900000000 ) {
        calFreq[0] = 0xE1;
        calFreq[1] = 0xE9;
    } else if( freq > 850000000 ) {
        calFreq[0] = 0xD7;
        calFreq[1] = 0xD8;
    } else if( freq > 770000000 ) {
        calFreq[0] = 0xC1;
        calFreq[1] = 0xC5;
    } else if( freq > 460000000 ) {
        calFreq[0] = 0x75;
        calFreq[1] = 0x81;
    } else if( freq > 425000000 ) {
        calFreq[0] = 0x6B;
        calFreq[1] = 0x6F;
    }
    WriteCommand( CALIBRATE_IMAGE, calFreq, 2 );
}


// TODO Unused function ?
void SX126x::CheckDeviceReady( void ) {
    WaitOnBusy( );
}


void SX126x::SetDioIrqParams(
                uint16_t irqMask,
                uint16_t dio1Mask,
                uint16_t dio2Mask,
                uint16_t dio3Mask
            ) {
    uint8_t buf[8];

    buf[0] = ( uint8_t )( ( irqMask >> 8 ) & 0x00FF );
    buf[1] = ( uint8_t )( irqMask & 0x00FF );
    buf[2] = ( uint8_t )( ( dio1Mask >> 8 ) & 0x00FF );
    buf[3] = ( uint8_t )( dio1Mask & 0x00FF );
    buf[4] = ( uint8_t )( ( dio2Mask >> 8 ) & 0x00FF );
    buf[5] = ( uint8_t )( dio2Mask & 0x00FF );
    buf[6] = ( uint8_t )( ( dio3Mask >> 8 ) & 0x00FF );
    buf[7] = ( uint8_t )( dio3Mask & 0x00FF );
    WriteCommand( SET_DIO_IRQ_PARAMS, buf, 8 );

}

void SX126x::ClearIrqStatus( uint16_t irq ) {
    uint8_t buf[2];

    buf[0] = ( uint8_t )( ( ( uint16_t )irq >> 8 ) & 0x00FF );
    buf[1] = ( uint8_t )( ( uint16_t )irq & 0x00FF );
    WriteCommand( CLR_IRQ_STATUS, buf, 2 );
}

void SX126x::SetBufferBaseAddress( uint8_t txBaseAddress, uint8_t rxBaseAddress ) {
    uint8_t buf[2];

    buf[0] = txBaseAddress;
    buf[1] = rxBaseAddress;
    WriteCommand( SET_BUFFER_BASE_ADDRESS, buf, 2 );
}


void SX126x::SetModulationParams(
                    Radio::Modulation_t modulation,
                    uint8_t SF,
                    Bandwith_t BW
                ) {
    uint8_t n;
    // uint32_t tempVal = 0;
    uint8_t buf[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    
    switch( modulation ) {
    case Radio::FSK:
        // TODO
        /*
        n = 8;
        tempVal = ( uint32_t )( 32 * ( ( double )XTAL_FREQ / ( double )modulationParams->Params.Gfsk.BitRate ) );
        buf[0] = ( tempVal >> 16 ) & 0xFF;
        buf[1] = ( tempVal >> 8 ) & 0xFF;
        buf[2] = tempVal & 0xFF;
        buf[3] = modulationParams->Params.Gfsk.ModulationShaping;
        buf[4] = modulationParams->Params.Gfsk.Bandwidth;
        tempVal = ( uint32_t )( ( double )modulationParams->Params.Gfsk.Fdev / ( double )FREQ_STEP );
        buf[5] = ( tempVal >> 16 ) & 0xFF;
        buf[6] = ( tempVal >> 8 ) & 0xFF;
        buf[7] = ( tempVal& 0xFF );
        SX126xWriteCommand( RADIO_SET_MODULATIONPARAMS, buf, n );
        */
        break;
    case Radio::LORA:
        n = 4;
        buf[0] = SF;
        buf[1] = BW + 4;
        // TODO - Need to be variable ?
        buf[2] = 0x01; // Coding rate = 4/5
        if( ( ( BW == BW_125 ) && ( ( SF == 11 ) || ( SF == 12 ) ) ) ||
            ( ( BW == BW_250 ) && ( SF == 12 ) ) ) {
            buf[3] = 0x01;
        } else {
            buf[3] = 0x00;
        }
        
        break;
    default:
        // TODO - Add error ?
        return;
    }
    
    WriteCommand( SET_MODULATION_PARAMS, buf, n );
}

void SX126x::SetPacketParams(
                Radio::Modulation_t modulation,
                uint8_t payloadSize
            ) {
    uint8_t n;
    // uint8_t crcVal = 0;
    uint8_t buf[9] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    
    switch( modulation )
    {
        case Radio::FSK:
        // TODO
        /*
        if( packetParams->Params.Gfsk.CrcLength == RADIO_CRC_2_BYTES_IBM )
        {
            SX126xSetCrcSeed( CRC_IBM_SEED );
            SX126xSetCrcPolynomial( CRC_POLYNOMIAL_IBM );
            crcVal = RADIO_CRC_2_BYTES;
        }
        else if( packetParams->Params.Gfsk.CrcLength == RADIO_CRC_2_BYTES_CCIT )
        {
            SX126xSetCrcSeed( CRC_CCITT_SEED );
            SX126xSetCrcPolynomial( CRC_POLYNOMIAL_CCITT );
            crcVal = RADIO_CRC_2_BYTES_INV;
        }
        else
        {
            crcVal = packetParams->Params.Gfsk.CrcLength;
        }
        n = 9;
        buf[0] = ( packetParams->Params.Gfsk.PreambleLength >> 8 ) & 0xFF;
        buf[1] = packetParams->Params.Gfsk.PreambleLength;
        buf[2] = packetParams->Params.Gfsk.PreambleMinDetect;
        buf[3] = ( packetParams->Params.Gfsk.SyncWordLength ); // convert from byte to bit - /!\ <<3 ?
        buf[4] = packetParams->Params.Gfsk.AddrComp;
        buf[5] = packetParams->Params.Gfsk.HeaderType;
        buf[6] = packetParams->Params.Gfsk.PayloadLength;
        buf[7] = crcVal;
        buf[8] = packetParams->Params.Gfsk.DcFree;
        */
        break;
    case Radio::LORA:
        n = 6;
        // TODO - Need to be variable ?
        buf[0] = 0x00;  // Preamble of 0x08 symbols
        buf[1] = 0x08;
        buf[2] = 0x00;  // Explicit header (Variable packet length)
        buf[3] = payloadSize;
        buf[4] = 0x01;  // Uplink: CRC ON
        buf[5] = 0x00;  // Uplink: Standard IQ
        break;
    default:
        // TODO - Add error ?
        return;
    }
    WriteCommand( SET_PACKET_PARAMS, buf, n );
}

void SX126x::SetPacketType( Radio::Modulation_t modulation ) {
    WriteCommand( SET_PACKET_TYPE, ( uint8_t* )&modulation, 1 );
}

void SX126x::SetPaConfig(
                    uint8_t paDutyCycle,
                    uint8_t hpMax,
                    uint8_t deviceSel,
                    uint8_t paLut
                ) {
    uint8_t buf[4];

    buf[0] = paDutyCycle;
    buf[1] = hpMax;
    buf[2] = deviceSel;
    buf[3] = paLut;
    WriteCommand( SET_PA_CONFIG, buf, 4 );
}


void SX126x::SetRfFrequency( uint32_t frequency ) {
    uint8_t buf[4];
    uint32_t freq = 0;
    
    // Set image calibration
    CalibrateImage( frequency );
    // Set frequency
    freq = ( uint32_t )( ( double )frequency / ( double )RADIO_FREQ_STEP );
    buf[0] = ( uint8_t )( ( freq >> 24 ) & 0xFF );
    buf[1] = ( uint8_t )( ( freq >> 16 ) & 0xFF );
    buf[2] = ( uint8_t )( ( freq >> 8 ) & 0xFF );
    buf[3] = ( uint8_t )( freq & 0xFF );
    WriteCommand( SET_RF_FREQUENCY, buf, 4 );
}

void SX126x::SetStandby( RadioStandbyModes_t standbyConfig ) {
    WriteCommand( SET_STANDBY, ( uint8_t* )&standbyConfig, 1 );
}


// TODO - Use µs
void SX126x::SetTx( uint32_t timeout ) {
    uint8_t buf[3];
    
    buf[0] = ( uint8_t )( ( timeout >> 16 ) & 0xFF );
    buf[1] = ( uint8_t )( ( timeout >> 8 ) & 0xFF );
    buf[2] = ( uint8_t )( timeout & 0xFF );
    WriteCommand( SET_TX, buf, 3 );
}


void SX126x::SetTxParams( int8_t power ) {
    uint8_t buf[2];
    
    // TODO - SX1262
    if( power == 15 ) {
        SX126x::SetPaConfig( 0x06, 0x00, 0x01, 0x01 );
    } else {
        SX126x::SetPaConfig( 0x04, 0x00, 0x01, 0x01 );
    }
    if( power > 14 ) {
        power = 14;
    } else if( power < -3 ) {
        power = -3;
    }
    buf[0] = 0x18;
    WriteRegisters( REG_OCP, buf, 1 ); // current max is 80 mA for the whole device
   
    buf[0] = power;
    // TODO - Variable ?
    buf[1] = 0x04; // Ramp time : 200 µs
    WriteCommand( SET_TX_PARAMS, buf, 2 );
}

void SX126x::WaitOnBusy( void ) {
    while( pinBusy == 1 ) { };
}

void SX126x::WriteBuffer( uint8_t offset, uint8_t *buffer, uint8_t size ) {
     // Wait on low busy
    CheckDeviceReady( );
    
    // Write buffers
    pinCS = 0;
    spi.write( WRITE_BUFFER );
    spi.write(  offset );
    for( uint16_t i = 0; i < size; i++ ) {
        spi.write( buffer[i] );
    }
    pinCS = 1;
    
    // TODO: WaitOnBusy( ); - useless here ?
}


void SX126x::WriteCommand( OpCode_t opCode, uint8_t *buffer, uint16_t size ) {
    // Wait on low busy
    CheckDeviceReady( );
    
    // Send command
    pinCS = 0;
    spi.write( (uint8_t) opCode );
    for( uint16_t i = 0; i < size; i++ ) {
        spi.write( buffer[i] );
    }
    pinCS = 1;
    
    // TODO: WaitOnBusy( ); - useless here ?
}

void SX126x::WriteRegisters( uint16_t address, uint8_t *buffer, uint16_t size ) {
    // Wait on low busy
    CheckDeviceReady( );
    
    // Write registers
    pinCS = 0;
    spi.write( WRITE_REGISTER );
    spi.write( ( address & 0xFF00 ) >> 8 );
    spi.write( address & 0x00FF );
    
    for( uint16_t i = 0; i < size; i++ ) {
       spi.write( buffer[i] );
    }
    
    pinCS = 1;
    
    // TODO: WaitOnBusy( ); - useless here ?
}


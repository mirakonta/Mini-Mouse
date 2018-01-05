/*

  __  __ _       _                                 
 |  \/  ( _)     ( _)                                
 | \  / |_ _ __  _ _ __ ___   ___  _   _ ___  ___  
 | |\/| | | '_ \| | '_ ` _ \ / _ \| | | / __|/ _ \
 | |  | | | | | | | | | | | | ( _) | |_| \__ \  __/ 
 |_|  |_|_|_| |_|_|_| |_| |_|\___/ \__,_|___/\___| 
                                                   
                                                   
Description       : LoraWan Mac Layer objets.  
License           : Revised BSD License, see LICENSE.TXT file include in the project
Maintainer        : Fabien Holin ( SEMTECH)
*/
#include "MacLayer.h"
#include "LoRaMacCrypto.h"
#include "LoraWanProcess.h"
#include "ApiRtc.h"
#include "Define.h"
#include "utilities.h"
#include "ApiFlash.h"

/*************************************************/
/*                     Constructors              */
/*@note have to check init values                */
/*************************************************/
LoraWanContainer::LoraWanContainer( PinName interrupt )
                    :Phy( interrupt ) { 
    FcntUp = 0;
    Phy.RadioContainerInit( );
    StateTimer = TIMERSTATE_SLEEP;
    FcntDwn = 0;
    MacTxPower = 14;
    AvailableRxPacketForUser = NOLORARXPACKETAVAILABLE;
    memcpy( appSKey, LoRaMacAppSKey, 16 );
    memcpy( nwkSKey, LoRaMacNwkSKey, 16 );
    DevAddr = LoRaDevAddr ;
    
}; 
LoraWanContainer::~LoraWanContainer( ) {
};


/***********************************************************************************************/
/*                      Public  Methods                                                        */
/***********************************************************************************************/
 //@note Partionning Public/private not yet finalized
 
/**********************************************************************/
/*                      Called During  LP.Process                     */
/**********************************************************************/
void LoraWanContainer::ConfigureRadioAndSend( void ) {
    RegionGiveNextDataRate ( ); // both choose  the next tx data rate but also compute the Sf and Bw (region dependant)
    uint8_t ChannelIndex = GiveNextChannel ( );//@note have to be completed
    Phy.DevAddrIsr    = DevAddr ;  //@note copy of the mac devaddr in order to filter it in the radio isr routine.
    Phy.TxFrequency   = MacTxFrequency[ChannelIndex]; 
    Phy.TxPower       = MacTxPower;
    Phy.TxSf          = MacTxSf;
    Phy.TxBw          = MacTxBw;
    Phy.SetTxConfig( );
    Phy.TxPayloadSize = MacPayloadSize;
    Phy.Send( );
};
void LoraWanContainer::ConfigureRadioForRx1 ( void ) {
    RegionSetRxConfig ( RX1 );
    Phy.RxFrequency   = Phy.TxFrequency;
    Phy.RxSf          = MacRx1Sf ;
    Phy.RxBw          = MacRx1Bw ;
    Phy.SetRxConfig( );
};

void LoraWanContainer::ConfigureRadioForRx2 ( void ) {
    RegionSetRxConfig ( RX2 );
    Phy.RxFrequency   = MacRx2Frequency;
    Phy.RxSf          = MacRx2Sf;
    Phy.RxBw          = MacRx2Bw;
    Phy.SetRxConfig( );
};

void LoraWanContainer::ConfigureTimerForRx ( int type ) {
    uint32_t tCurrentMillisec;
    int tAlarmMillisec;
    uint64_t tAlarm64bits;
    int toffset = 8;  // @note created a Define?  
    tCurrentMillisec =  RtcGetTimeMs( &tAlarm64bits);
    if (type == RX1) {
        tAlarmMillisec = ( MacRx1Delay * 1000 )+ Phy.TimestampRtcIsr - tCurrentMillisec - toffset ;
        if ( tAlarmMillisec <= toffset ) {// too late to launch a timer
            Phy.StateRadioProcess = RADIOSTATE_RX1FINISHED ;
        } else { 
            SetAlarm( tAlarmMillisec );
        }
    } else {
        tAlarmMillisec = ( MacRx1Delay * 1000 ) + 1000 + Phy.TimestampRtcIsr - tCurrentMillisec - toffset ;// @note Rx2 Dalay is alway RX1DELAY + 1 second
        if ( tAlarmMillisec <= toffset ) {// too late to launch a timer
            Phy.StateRadioProcess = RADIOSTATE_IDLE ;
            pcf.printf( " error case negative Timer %d ms\n", tAlarmMillisec );
        } else { 
            SetAlarm( tAlarmMillisec );
        }
    }
    pcf.printf( " timer will expire in %d ms\n", tAlarmMillisec );
}

    /****************************/
    /*       DecodeRxFrame      */
    /****************************/
eRxPacketType LoraWanContainer::DecodeRxFrame( void ) {

    int status = OKLORAWAN ;
    eRxPacketType RxPacketType = NOMOREVALIDRXPACKET ; 
    uint32_t micIn ;
    status += CheckRxPayloadLength ( );
    status += ExtractRxMhdr ( ) ;
        /************************************************************************/
        /*                 Case : the receive packet is a JoinResponse          */
        /************************************************************************/
    if ( MtypeRx == JOINACCEPT ) {
        LoRaMacJoinDecrypt( &Phy.RxPhyPayload[1], Phy.RxPhyPayloadSize-1, LoRaMacAppKey, &MacRxPayload[1] );
        MacRxPayload[0] =  Phy.RxPhyPayload[0];
        MacRxPayloadSize = Phy.RxPhyPayloadSize - MICSIZE ;
        memcpy((uint8_t *)&micIn, &MacRxPayload[MacRxPayloadSize], MICSIZE);
        status += LoRaMacCheckJoinMic( MacRxPayload, MacRxPayloadSize, LoRaMacAppKey, micIn);
        if ( status == OKLORAWAN) {
            return JOINACCEPTPACKET;
        }
    } else {
        /************************************************************************/
        /*               Case : the receive packet is not a JoinResponse        */
        /************************************************************************/
        uint16_t FcntDownTmp = 0;
        status += ExtractRxFhdr ( &FcntDownTmp) ;
        if ( status == OKLORAWAN) {
            MacRxPayloadSize = Phy.RxPhyPayloadSize - MICSIZE ;
            memcpy((uint8_t *)&micIn, &Phy.RxPhyPayload[MacRxPayloadSize], MICSIZE);
            status += LoRaMacCheckMic(&Phy.RxPhyPayload[0], MacRxPayloadSize, nwkSKey, DevAddr, FcntDownTmp, micIn ); // @note api discussion see at the end of this file
        }
        if ( status == OKLORAWAN) {
            status = AcceptFcntDwn ( FcntDownTmp ) ;
        }
        if ( status == OKLORAWAN) {
            MacRxPayloadSize = MacRxPayloadSize - FHDROFFSET - FoptsLength ;
            if ( FportRx == 0 ) {
                LoRaMacPayloadDecrypt( &Phy.RxPhyPayload[FHDROFFSET + FoptsLength], MacRxPayloadSize, nwkSKey, DevAddr, 1, FcntDwn, &MacNwkPayload[0] );
                MacNwkPayloadSize = MacRxPayloadSize;
                RxPacketType = NWKRXPACKET ;
            } else {
                LoRaMacPayloadDecrypt( &Phy.RxPhyPayload[FHDROFFSET + FoptsLength], MacRxPayloadSize, appSKey, DevAddr, 1, FcntDwn, &MacRxPayload[0] );
                if ( FoptsLength != 0 ) {
                    memcpy ( MacNwkPayload, Fopts, FoptsLength);
                    MacNwkPayloadSize = FoptsLength;
                    RxPacketType = USERRX_FOPTSPACKET ;
                } 
                if ( MacRxPayloadSize > 0 ) {
                    AvailableRxPacketForUser = LORARXPACKETAVAILABLE; 
                }
            }
            pcf.printf( " MtypeRx = %d \n ",MtypeRx );
            pcf.printf( " FcntDwn = %d \n ",FcntDwn );
        }
    }
    return ( RxPacketType );
}
    /*********************************/
    /*      End of Decode Frame      */
    /*********************************/
    /************************************************************************************************/
    /*                      NWK MANAGEMENTS Methods                                                 */
    /************************************************************************************************/
eStatusLoRaWan LoraWanContainer::ParseManagementPacket( void ) {
    uint8_t CmdIdentifier;
    eStatusLoRaWan status = OKLORAWAN ;
    NwkPayloadIndex = 0;
    MacNwkAnsSize = 0;
    uint8_t MaxCmdNum = 16 ; //@note security to avoid an infinite While erro 
    while ( ( MacNwkPayloadSize > NwkPayloadIndex ) && (  MaxCmdNum > 0 ) ) { //@note MacNwkPayloadSize and MacNwkPayload[0] are updated in Parser's method
        MaxCmdNum --; 
        if ( MaxCmdNum == 0 ) {
            return ( ERRORLORAWAN );
        }
        CmdIdentifier = MacNwkPayload[NwkPayloadIndex];
        switch ( CmdIdentifier ) {
            case LINK_CHECK_ANS :
                LinkCheckParser( );
                break;
            case LINK_ADR_REQ :
                LinkADRParser( );
                break;
            case DUTY_CYCLE_REQ :
                DutyCycleParser( );
                break;
            case RXPARRAM_SETUP_REQ :
                RXParamSetupParser( );
                break;
            case DEV_STATUS_REQ :
                DevStatusParser( );
                break;
            case NEW_CHANNEL_REQ :
                NewChannelParser( );
                break;
            case RXTIMING_SETUP_REQ :
                RXTimingSetupParser( );
                break;
        }
    }
    return ( status ); 
}

void LoraWanContainer::UpdateMacLayer ( void ) {
    FcntUp++; //@note case Retry not yet manage
    /*Store Context In ROM */
    if (( FcntUp % FLASH_UPDATE_PERIOD ) == 0 ){
        SaveInFlash ( );
    }
        
    switch ( IsFrameToSend ) {
        case NOFRAME_TOSEND :

            break;
        case NWKFRAME_TOSEND :
            memcpy( &Phy.TxPhyPayload[FHDROFFSET], MacNwkAns, MacNwkAnsSize );
            UserPayloadSize = MacNwkAnsSize;
            fPort = PORTNWK;
            MType = UNCONFDATAUP; //@note Mtype have to be confirm 
            BuildTxLoraFrame( );
            EncryptTxFrame( );
            
            break;
        case USERACK_TOSEND :

            break;
    }
}
    /**********************************************************************/
    /*                      Special Case Join OTA                         */
    /**********************************************************************/
void LoraWanContainer::UpdateJoinProcedure ( void ) { //@note tbd add valid test 
    uint8_t AppNonce[6];
    memcpy( AppNonce, &MacRxPayload[1], 6 );
    LoRaMacJoinComputeSKeys(LoRaMacAppKey, AppNonce, DevNonce,  nwkSKey, appSKey );
    for( int i = 0 ;i<16;i++) {
        
    }
    DevAddr              = MacRxPayload[7] + ( MacRxPayload[8] << 8 ) + ( MacRxPayload[9] << 16 )+ ( MacRxPayload[10] << 24 );
    Phy.DevAddrIsr       = DevAddr ; 
    MacRx1DataRateOffset = ( MacRxPayload[11] & 0x70 ) >> 3;
    MacRx2Sf             = ( MacRxPayload[11] & 0x0F );
    MacRx1Delay          = MacRxPayload[12];
    Phy.JoinedStatus = JOINED;
    //@note have to manage option byte for channel frequency planned
}
/**********************************************************************/
/*                    End Of Called During  LP.Process                */
/**********************************************************************/





/********************************************************/
/*               Called During LP.Join()                */
/********************************************************/

void LoraWanContainer::BuildJoinLoraFrame( void ) {
    DevNonce = randr( 0, 65535 )+18714;
    MType = JOINREQUEST ;
    SetMacHeader ( );
    for (int i = 0; i <8; i++){ 
        Phy.TxPhyPayload[1+i] = AppEui[7-i];
        Phy.TxPhyPayload[9+i] = DevEui[7-i];
    }
    Phy.TxPhyPayload[17] = ( uint8_t )( ( DevNonce & 0x00FF ) );
    Phy.TxPhyPayload[18] = ( uint8_t )( ( DevNonce & 0xFF00 ) >> 8 );
    MacPayloadSize = 19 ;
    uint32_t mic ; 
    LoRaMacJoinComputeMic( &Phy.TxPhyPayload[0], MacPayloadSize, LoRaMacAppKey, &mic );
    memcpy(&Phy.TxPhyPayload[MacPayloadSize], (uint8_t *)&mic, 4);
    MacPayloadSize = MacPayloadSize + 4;
}


/********************************************************/
/*               Called During LP.Send ()               */
/********************************************************/

void LoraWanContainer::BuildTxLoraFrame( void ) {
    Fctrl = 0;  // @todo in V1.0 Adr isn't manage and ack is done by an empty packet
    Fctrl = ( AckBitForTx << 5 );
    AckBitForTx = 0;
    SetMacHeader( );
    SetFrameHeader( );
    MacPayloadSize = UserPayloadSize+FHDROFFSET; 
};
void LoraWanContainer::EncryptTxFrame( void ) {
    LoRaMacPayloadEncrypt( &Phy.TxPhyPayload[FHDROFFSET], UserPayloadSize, (fPort == PORTNWK)? nwkSKey :appSKey, DevAddr, UP_LINK, FcntUp, &Phy.TxPhyPayload[FHDROFFSET] );
    LoRaMacComputeAndAddMic( &Phy.TxPhyPayload[0], MacPayloadSize, nwkSKey, DevAddr, UP_LINK, FcntUp );
    MacPayloadSize = MacPayloadSize + 4;
};



/************************************************************************************************/
/*                      Private  Methods                                                        */
/************************************************************************************************/
void LoraWanContainer::SetMacHeader( void ) {
    Phy.TxPhyPayload[0] = ( ( MType & 0x7 ) <<5 ) + ( MajorBits & 0x3 );
};
void LoraWanContainer::SetFrameHeader( ) {
    Phy.TxPhyPayload[1] = ( uint8_t )( ( DevAddr & 0x000000FF ) );
    Phy.TxPhyPayload[2] = ( uint8_t )( ( DevAddr & 0x0000FF00 ) >> 8 );
    Phy.TxPhyPayload[3] = ( uint8_t )( ( DevAddr & 0x00FF0000 ) >> 16 );
    Phy.TxPhyPayload[4] = ( uint8_t )( ( DevAddr & 0xFF000000 ) >> 24 );
    Phy.TxPhyPayload[5] = Fctrl;
    Phy.TxPhyPayload[6] = ( uint8_t )( ( FcntUp & 0x00FF ) );
    Phy.TxPhyPayload[7] = ( uint8_t )( ( FcntUp & 0x00FF00 ) >> 8 );
    Phy.TxPhyPayload[8] = fPort;
}

int LoraWanContainer::CheckRxPayloadLength ( void ) {
    int status = OKLORAWAN;
    if ( Phy.RxPhyPayloadSize < MINLORAWANPAYLOADSIZE ) {
        status = ERRORLORAWAN;
        return (status);
    }
    return (status);
}

int LoraWanContainer::ExtractRxMhdr ( void ) {
    int status = OKLORAWAN; 
    MtypeRx = Phy.RxPhyPayload[0] >> 5 ;
    MajorRx =  Phy.RxPhyPayload[0] & 0x3 ;
    if (( MtypeRx == JOINREQUEST) || ( MtypeRx == UNCONFDATAUP ) || ( MtypeRx == CONFDATAUP) || ( MtypeRx == REJOINREQUEST )) {
        status = ERRORLORAWAN;
        DEBUG_MSG( " BAD RX MHDR\n " );
    }
    AckBitForTx = ( MtypeRx == CONFDATADOWN ) ? 1 : 0 ;
        
    return (status);
}

int LoraWanContainer::ExtractRxFhdr ( uint16_t *FcntDwnTmp ) { //@note Not yet at all finalized have to initiate action on each field
    int status = OKLORAWAN; 
    uint32_t DevAddrtmp = 0 ;
    DevAddrtmp = Phy.RxPhyPayload[1] + ( Phy.RxPhyPayload[2] << 8 ) + ( Phy.RxPhyPayload[3] << 16 )+ ( Phy.RxPhyPayload[4] << 24 );
    status = (DevAddrtmp == DevAddr) ? OKLORAWAN : ERRORLORAWAN;
    FctrlRx = Phy.RxPhyPayload[5] ;
    *FcntDwnTmp = Phy.RxPhyPayload[6] + ( Phy.RxPhyPayload[7] << 8 );
    FoptsLength = FctrlRx & 0x0F;
    memcpy(&Fopts[0], &Phy.RxPhyPayload[8], FoptsLength);
    for (int i = 0 ; i < FoptsLength ; i ++) {
        DEBUG_PRINTF( " %x \n", Fopts[i] );
    }
    FportRx = Phy.RxPhyPayload[8+FoptsLength];
    /**************************/
    /* manage Fctrl Byte      */
    /**************************/

    return (status);
}
int LoraWanContainer::AcceptFcntDwn ( uint16_t FcntDwnTmp ) {
    int status = OKLORAWAN; 
    uint16_t FcntDwnLsb = ( FcntDwn & 0x0000FFFF );
    uint16_t FcntDwnMsb = ( FcntDwn & 0xFFFF0000 ) >> 16;
#ifdef CHECKFCNTDOWN 
    if  ( FcntDwnmtp > FcntDwnLsb ) {
        FcntDwn = FcntDwnTmp ;
    } else if ( ( FcntDwnLsb - FcntDwnmtp ) > MAX_FCNT_GAP ) ) {
        FcntDwn = ( ( FcntDwnMsb + 1 ) << 16 ) + FcntDwnTmp ;
    } else {
         status = ERRORLORAWAN ;
    }
        
#else
    if ( ( FcntDwnLsb - FcntDwnTmp ) > MAX_FCNT_GAP )  {
        FcntDwn = ( ( FcntDwnMsb + 1 ) << 16 ) + FcntDwnTmp ;
    } else {
        FcntDwn = FcntDwnTmp ;
    } 
#endif
    pcf.printf("fcntdwn = %d\n",status);
    return ( status ) ;
}


uint8_t  LoraWanContainer::GiveNextChannel( void ) {
    return ( randr( 0, NbOfActiveChannel - 1 ) );

};
/************************************************************************************************/
/*                    Private NWK MANAGEMENTS Methods                                           */
/************************************************************************************************/

void LoraWanContainer::LinkCheckParser( void ) {
    
    //@NOTE NOT YET IMPLEMENTED
}
    /*****************************************************/
    /*    Private NWK MANAGEMENTS : LinkADR              */
    /*****************************************************/
void LoraWanContainer::LinkADRParser( void ) {
    DEBUG_PRINTF (" %x %x %x %x \n", MacNwkPayload[ NwkPayloadIndex + 1], MacNwkPayload[NwkPayloadIndex + 2], MacNwkPayload[NwkPayloadIndex + 3], MacNwkPayload[NwkPayloadIndex + 4] );
    int status = OKLORAWAN;
    uint8_t StatusAns = 0x7 ; // initilised for ans answer ok 
    uint8_t temp ; 
    uint16_t temp2 ; 
     /* Valid DataRate  And Prepare Ans */
    temp = ( ( MacNwkPayload[ NwkPayloadIndex + 1 ] & 0xF0 ) >> 4 );
    status = isValidDataRate( temp );
    (status == OKLORAWAN ) ? MacTxDataRateAdr = temp : StatusAns &= 0x5 ; 
    
    /* Valid TxPower  And Prepare Ans */
    temp = ( MacNwkPayload[ NwkPayloadIndex + 1 ] & 0x0F );
    status = OKLORAWAN;
    status = isValidTxPower( temp );
    if (status == OKLORAWAN ) {
        RegionSetPower ( temp );
    } else {
        StatusAns &= 0x3 ; 
    }
     /* Valid DataRate Channel Mask And Prepare Ans */
     temp2 = MacNwkPayload[ NwkPayloadIndex + 2 ] + ( MacNwkPayload[ NwkPayloadIndex + 3 ] << 8 )  ;
    (status == OKLORAWAN ) ? MacChMask = temp2 : StatusAns &= 0x6 ; 
    
    /* Valid Redundancy And Prepare Ans */
     temp = ( MacNwkPayload[ NwkPayloadIndex + 4] & 0x0F );
     (temp >0 ) ? MacNbRepUnconfirmedTx = temp : StatusAns &= 0x6 ; 
    
    /* Prepare Ans*/
    MacNwkAns [ MacNwkAnsSize ] = LINK_ADR_ANS ; // copy Cid
    MacNwkAns [ MacNwkAnsSize + 1 ] = StatusAns ;
    MacNwkAnsSize   += LINK_ADR_ANS_SIZE ;
    NwkPayloadIndex += LINK_ADR_REQ_SIZE;
    IsFrameToSend = NWKFRAME_TOSEND ;
}

void LoraWanContainer::DutyCycleParser( void ) {
    //@NOTE NOT YET IMPLEMENTED
}
    /*****************************************************/
    /*    Private NWK MANAGEMENTS : RXParamSetupParser   */
    /*****************************************************/
void LoraWanContainer::RXParamSetupParser( void ) {
    DEBUG_PRINTF (" %x %x %x %x \n", MacNwkPayload[ NwkPayloadIndex + 1], MacNwkPayload[NwkPayloadIndex + 2], MacNwkPayload[NwkPayloadIndex + 3], MacNwkPayload[NwkPayloadIndex + 4] );
    int status = OKLORAWAN;
    uint8_t StatusAns = 0x7 ; // initilised for ans answer ok 
    uint32_t temp ; 
    /* Valid Rx1DrOffset And Prepare Ans */
    temp = ( MacNwkPayload[ NwkPayloadIndex + 1 ] & 0x70 ) >> 3 ;
    status = isValidRx1DrOffset( temp );
    (status == OKLORAWAN ) ? MacRx1DataRateOffset = temp : StatusAns &= 0x6 ; 
    
    /* Valid MacRx2Dr And Prepare Ans */
    status = OKLORAWAN;
    temp = ( MacNwkPayload[ NwkPayloadIndex + 1 ] & 0x0F );
    status = isValidDataRate( temp );
    (status == OKLORAWAN ) ? MacRx2DataRate = temp : StatusAns &= 0x5 ; 
    
    /* Valid MacRx2Frequency And Prepare Ans */
    status = OKLORAWAN;
    temp = ( MacNwkPayload[ NwkPayloadIndex + 2 ] ) + ( MacNwkPayload[ NwkPayloadIndex + 3 ] << 8 ) + ( MacNwkPayload[ NwkPayloadIndex + 4 ] << 16 );
    status = isValidMacFrequency ( temp );
    (status == OKLORAWAN ) ? MacRx2Frequency = temp * 100 : StatusAns &= 0x3 ; 
    
    /* Prepare Ans*/
    MacNwkAns [ MacNwkAnsSize ] = RXPARRAM_SETUP_ANS ; // copy Cid
    MacNwkAns [ MacNwkAnsSize + 1 ] = StatusAns ;
    MacNwkAnsSize += RXPARRAM_SETUP_ANS_SIZE ;
    NwkPayloadIndex += RXPARRAM_SETUP_REQ_SIZE;
    IsFrameToSend = NWKFRAME_TOSEND ;
}

void LoraWanContainer::DevStatusParser( void ) {
    //@NOTE NOT YET IMPLEMENTED
}
void LoraWanContainer::NewChannelParser( void ) {
    //@NOTE NOT YET IMPLEMENTED
}
void LoraWanContainer::RXTimingSetupParser( void ) {
    //@NOTE NOT YET IMPLEMENTED
}


uint8_t LoraWanContainer::isValidChannelMask ( uint16_t temp ) {
    // @note not yet implemented 
    return ( OKLORAWAN );
}
uint8_t LoraWanContainer::isValidNbRep ( uint8_t temp ) {
    // @note not yet implemented 
    return ( OKLORAWAN );
}
void LoraWanContainer::SaveInFlash ( ) {
    BackUpFlash.MacTxDataRate           = MacTxDataRate;
    BackUpFlash.MacTxPower              = MacTxPower;
    BackUpFlash.MacChMask               = MacChMask;
    BackUpFlash.MacNbRepUnconfirmedTx   = MacNbRepUnconfirmedTx; 
    BackUpFlash.MacRx2Frequency         = MacRx2Frequency; 
    BackUpFlash.MacRx2DataRate          = MacRx2DataRate;
    BackUpFlash.MacRx1DataRateOffset    = MacRx1DataRateOffset;
    BackUpFlash.NbOfActiveChannel       = NbOfActiveChannel;
    BackUpFlash.MacRx1Delay             = MacRx1Delay;
    BackUpFlash.FcntUp                  = FcntUp;
    BackUpFlash.FcntDwn                 = FcntDwn;
    BackUpFlash.DevAddr                 = DevAddr;
    BackUpFlash.JoinedStatus            = Phy.JoinedStatus;
    memcpy( &BackUpFlash.MacTxFrequency[0], &MacTxFrequency[0], 16);
    memcpy( &BackUpFlash.MacMinMaxDataRateChannel[0], &MacMinMaxDataRateChannel[0], 16);
    memcpy( &BackUpFlash.nwkSKey[0], &nwkSKey[0], 16);
    memcpy( &BackUpFlash.appSKey[0], &appSKey[0], 16);
    gFlash.StoreContext( &BackUpFlash, USERFLASHADRESS, sizeof(sBackUpFlash) );    
}


void LoraWanContainer::LoadFromFlash ( ) {
    gFlash.RestoreContext((uint8_t *)(&BackUpFlash), USERFLASHADRESS, sizeof(sBackUpFlash));
    BackUpFlash.FcntUp            +=  FLASH_UPDATE_PERIOD; //@note automatic increment
    MacTxDataRate                 = BackUpFlash.MacTxDataRate;
    MacTxPower                    = BackUpFlash.MacTxPower;
    MacChMask                     = BackUpFlash.MacChMask;
    MacNbRepUnconfirmedTx         = BackUpFlash.MacNbRepUnconfirmedTx; 
    MacRx2Frequency               = BackUpFlash.MacRx2Frequency; 
    MacRx2DataRate                = BackUpFlash.MacRx2DataRate;
    MacRx1DataRateOffset          = BackUpFlash.MacRx1DataRateOffset;
    NbOfActiveChannel             = BackUpFlash.NbOfActiveChannel;
    MacRx1Delay                   = BackUpFlash.MacRx1Delay ;
    FcntUp                        = BackUpFlash.FcntUp ;
    FcntDwn                       = BackUpFlash.FcntDwn ;
    DevAddr                       = BackUpFlash.DevAddr;
    Phy.JoinedStatus              = ( eJoinStatus ) BackUpFlash.JoinedStatus;
    memcpy( &MacTxFrequency[0], &BackUpFlash.MacTxFrequency[0], 16);
    memcpy( &MacMinMaxDataRateChannel[0], &BackUpFlash.MacMinMaxDataRateChannel[0], 16);
    memcpy( &nwkSKey[0], &BackUpFlash.nwkSKey[0], 16);
    memcpy( &appSKey[0], &BackUpFlash.appSKey[0], 16); 
    gFlash.StoreContext( &BackUpFlash, USERFLASHADRESS, sizeof(sBackUpFlash) );    
}
/**************************************TIMER PART**********************************************************/
/**********************************************************************************************************/
/*@Note Probably to create a new directory or may be an timer Object to be discuss                        */

 
/************************************************************************************/
/*                                Rx1 Timer Isr Routine                             */
/*                               Called when Alarm expires                          */
/************************************************************************************/
void LoraWanContainer::SetAlarm (uint32_t alarmInMs) {
    TimerLora.attach_us(this, &LoraWanContainer::IsrTimerRx, alarmInMs * 1000);
}

void LoraWanContainer::IsrTimerRx( void ) {
     StateTimer = TIMERSTATE_SLEEP;
     Phy.Radio.Rx(0); //@note No More timeout FW on RX use only timeout impplement in the HW radio
};
 

/****************************************API Crypto ***********************/
//uint8_t LoraWanContainer::crypto_verifyMICandDecrypt( uint8_t *frame_header, const uint8_t *encrypted_payload ,uint32_t micIn, uint8_t keySet, uint8_t *decrypted_payload, uint8_t PayloadSize){
//    uint32_t DevAddrtmp = 0;
//    uint16_t FcntDwnmtp = 0;
//    int status = OKLORAWAN ;
//    if ( keySet == UNICASTKEY) {
//        DevAddrtmp = frame_header[1] + ( frame_header[2] << 8 ) + ( frame_header[3] << 16 )+ ( frame_header[4] << 24 );
//        FcntDwnmtp = frame_header[6] + ( frame_header[7] << 8 );
//        status += LoRaMacCheckMic(frame_header, PayloadSize, nwkSKey, DevAddrtmp, FcntDwnmtp, micIn );
//        
//        //@note note sure that it is better to  sen dheader because extract again devaddr fcntdwn fopts etc ....
//        
//        PayloadSize = PayloadSize - FHDROFFSET - FoptsLength ;
//        if ( status == OKLORAWAN) {
//            LoRaMacPayloadDecrypt( &Phy.RxPhyPayload[FHDROFFSET + FoptsLength], MacRxPayloadSize, (FportRx == 0 )?nwkSKey:appSKey, DevAddr, 1, FcntDwn, &MacRxPayload[0] );
//        }
//    }
//    return ( status );
//}

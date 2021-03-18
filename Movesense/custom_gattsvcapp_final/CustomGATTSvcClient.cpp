#include "movesense.h"

#include "CustomGATTSvcClient.h"
#include "common/core/debug.h"
#include "oswrapper/thread.h"

#include "comm_ble_gattsvc/resources.h"
#include "comm_ble/resources.h"
#include "meas_temp/resources.h"
#include "meas_ecg/resources.h"
#include "meas_hr/resources.h"
#include "movesense_types/resources.h"

#include <algorithm>
#include <iterator>

const char* const CustomGATTSvcClient::LAUNCHABLE_NAME = "CstGattS";

const uint16_t measCharUUID16 = 0x2A1C;
const uint16_t intervalCharUUID16 = 0x2A21;

const int DEFAULT_MEASUREMENT_INTERVAL_SECS = 1000;

CustomGATTSvcClient::CustomGATTSvcClient()
: ResourceClient(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION),
LaunchableModule(LAUNCHABLE_NAME, WB_EXEC_CTX_APPLICATION),
mMeasIntervalSecs(DEFAULT_MEASUREMENT_INTERVAL_SECS),
mTemperatureSvcHandle(0),
mMeasCharHandle(0),
mIntervalCharHandle(0),
mIntervalCharResource(whiteboard::ID_INVALID_RESOURCE),
mMeasCharResource(whiteboard::ID_INVALID_RESOURCE),
mMeasurementTimer(whiteboard::ID_INVALID_TIMER)
{
}

CustomGATTSvcClient::~CustomGATTSvcClient()
{
}

bool CustomGATTSvcClient::initModule()
{
    mModuleState = WB_RES::ModuleStateValues::INITIALIZED;
    return true;
}

void CustomGATTSvcClient::deinitModule()
{
    mModuleState = WB_RES::ModuleStateValues::UNINITIALIZED;
}

bool CustomGATTSvcClient::startModule()
{
    mModuleState = WB_RES::ModuleStateValues::STARTED;
    
    // Configure custom gatt service
    configGattSvc();
    return true;
}

/** @see whiteboard::ILaunchableModule::startModule */
void CustomGATTSvcClient::stopModule()
{
    // Stop timer if running
    if (mMeasurementTimer != whiteboard::ID_INVALID_TIMER)
        stopTimer(mMeasurementTimer);
    
    // Unsubscribe if needed
    if (mIntervalCharResource != whiteboard::ID_INVALID_RESOURCE)
        asyncUnsubscribe(mIntervalCharResource);
    if (mMeasCharResource != whiteboard::ID_INVALID_RESOURCE)
        asyncUnsubscribe(mMeasCharResource);
    
    mMeasurementTimer = whiteboard::ID_INVALID_TIMER;
}


void CustomGATTSvcClient::configGattSvc() {
    WB_RES::GattSvc customGattSvc;
    WB_RES::GattChar characteristics[2];
    WB_RES::GattChar &measChar = characteristics[0];
    WB_RES::GattChar &intervalChar = characteristics[1];
    
    const uint16_t healthThermometerSvcUUID16 = 0x1809;
    
    // Define the CMD characteristics
    WB_RES::GattProperty measCharProp = WB_RES::GattProperty::NOTIFY;
    WB_RES::GattProperty intervalCharProps[2] = {WB_RES::GattProperty::READ, WB_RES::GattProperty::WRITE};
    
    measChar.props = whiteboard::MakeArray<WB_RES::GattProperty>( &measCharProp, 1);
    measChar.uuid = whiteboard::MakeArray<uint8_t>( reinterpret_cast<const uint8_t*>(&measCharUUID16), 2);
    
    intervalChar.props = whiteboard::MakeArray<WB_RES::GattProperty>( intervalCharProps, 2);
    intervalChar.uuid = whiteboard::MakeArray<uint8_t>( reinterpret_cast<const uint8_t*>(&intervalCharUUID16), 2);
    intervalChar.initial_value = whiteboard::MakeArray<uint8_t>( reinterpret_cast<const uint8_t*>(&mMeasIntervalSecs), 2);
    
    // Combine chars to service
    customGattSvc.uuid = whiteboard::MakeArray<uint8_t>( reinterpret_cast<const uint8_t*>(&healthThermometerSvcUUID16), 2);
    customGattSvc.chars = whiteboard::MakeArray<WB_RES::GattChar>(characteristics, 2);
    
    // Create custom service
    asyncPost(WB_RES::LOCAL::COMM_BLE_GATTSVC(), AsyncRequestOptions::Empty, customGattSvc);
}

void CustomGATTSvcClient::onTimer(whiteboard::TimerId timerId)
{
    DEBUGLOG("CustomGATTSvcClient::onTimer");
    //asyncGet(WB_RES::LOCAL::MEAS_ECG_INFO(), NULL);
}

#include <math.h>
static void floatToFLOAT(uint16_t value, uint8_t* bufferOut)
{
    uint16_t mantInt24;
    mantInt24 = value;
    bufferOut[0] = (uint8_t)(mantInt24 & 0xff);
    bufferOut[1] = (uint8_t)((mantInt24>>8) & 0xff);
    bufferOut[2] = (uint8_t)((mantInt24>>16) & 0xff);
    bufferOut[3] = (uint8_t)((mantInt24>>24) & 0xff);
}

static void floatToFLOAT1(uint16_t value, uint8_t* bufferOut)
{
    uint16_t mantInt24;
    mantInt24 = value;
    bufferOut[1] = (uint8_t)(mantInt24 & 0xff);
    bufferOut[0] = (uint8_t)((mantInt24>>8) & 0xff);
}

static void floatToFLOAT2(int32_t* value, uint8_t* bufferOut)
{
    int32_t mantInt24 = (int32_t)value[0];
    mantInt24 = -mantInt24 + 1;
    bufferOut[1] = (uint8_t)(mantInt24 & 0xff);
    bufferOut[0] = (uint8_t)((mantInt24>>8) & 0xff);
}

static void floatToFLOAT3(int16_t* value, uint8_t* bufferOut)
{
    int16_t mantInt24 = (int16_t)value[0];
    //mantInt24 = value[0];
    bufferOut[0] = (uint8_t)(mantInt24 & 0xff);
    bufferOut[1] = (uint8_t)((mantInt24>>8) & 0xff);
}

static void floatToFLOAT4(uint32_t value, uint8_t* bufferOut)
{
    uint32_t mantInt24 = value;
    //mantInt24 = value[0];
    bufferOut[3] = (uint8_t)(mantInt24 & 0xff);
    bufferOut[2] = (uint8_t)((mantInt24>>8) & 0xff);
    bufferOut[1] = (uint8_t)((mantInt24>>16) & 0xff);
    bufferOut[0] = (uint8_t)((mantInt24>>24) & 0xff);
}

void CustomGATTSvcClient::onGetResult(whiteboard::RequestId requestId, whiteboard::ResourceId resourceId, whiteboard::Result resultCode, const whiteboard::Value& rResultData)
{
    DEBUGLOG("CustomGATTSvcClient::onGetResult");
    switch(resourceId.localResourceId)
    {
        case WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE::LID:
        {
            const WB_RES::GattSvc &svc = rResultData.convertTo<const WB_RES::GattSvc &>();
            for (size_t i=0; i<svc.chars.size(); i++) {
                const WB_RES::GattChar &c = svc.chars[i];
                uint16_t uuid16 = *reinterpret_cast<const uint16_t*>(&(c.uuid[0]));
                
                if(uuid16 == measCharUUID16)
                    mMeasCharHandle = c.handle.hasValue() ? c.handle.getValue() : 0;
                else if(uuid16 == intervalCharUUID16)
                    mIntervalCharHandle = c.handle.hasValue() ? c.handle.getValue() : 0;
            }
            
            if (!mIntervalCharHandle || !mMeasCharHandle)
            {
                DEBUGLOG("ERROR: Not all chars were configured!");
                return;
            }
            
            char pathBuffer[32]= {'\0'};
            snprintf(pathBuffer, sizeof(pathBuffer), "/Comm/Ble/GattSvc/%d/%d", mTemperatureSvcHandle, mIntervalCharHandle);
            getResource(pathBuffer, mIntervalCharResource);
            snprintf(pathBuffer, sizeof(pathBuffer), "/Comm/Ble/GattSvc/%d/%d", mTemperatureSvcHandle, mMeasCharHandle);
            getResource(pathBuffer, mMeasCharResource);
            
            // Subscribe to listen to intervalChar notifications (someone writes new value to intervalChar)
            asyncSubscribe(mIntervalCharResource, AsyncRequestOptions::Empty);
            // Subscribe to listen to measChar notifications (someone enables/disables the NOTIFY characteristic)
            asyncSubscribe(mMeasCharResource, AsyncRequestOptions::Empty);
        }
            break;
        case WB_RES::LOCAL::MEAS_ECG_INFO::LID:
        {
            WB_RES::ECGInfo value = rResultData.convertTo<WB_RES::ECGInfo>();
            //WB_RES::TemperatureValue value = rResultData.convertTo<WB_RES::TemperatureValue>();
            uint16_t ecgsamplerate = value.currentSampleRate;
            uint16_t ecgarraysize = value.arraySize;
            int16_t ecgavailablesamp[2] = {};
            std::copy(std::begin(value.availableSampleRates), std::end(value.availableSampleRates), std::begin(ecgavailablesamp));
            // int32_t ecgarray = value.measurement;
            
            // Return data
            uint8_t buffer[5]; // 1 byte or flags, 4 for FLOAT "in Celsius" value
            buffer[0]=0;
            
            
            floatToFLOAT(ecgsamplerate, &buffer[1]);
            floatToFLOAT1(ecgarraysize, &buffer[2]);
            floatToFLOAT3(ecgavailablesamp, &buffer[3]);
            
            // Write the result to measChar. This results NOTIFY to be triggered in GATT service
            WB_RES::Characteristic newMeasCharValue;
            newMeasCharValue.bytes = whiteboard::MakeArray<uint8_t>(buffer, sizeof(buffer));
            asyncPut(WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE_CHARHANDLE(), AsyncRequestOptions::Empty, mTemperatureSvcHandle, mMeasCharHandle, newMeasCharValue);
        }
            break;
    }
}

void CustomGATTSvcClient::onNotify(whiteboard::ResourceId resourceId, const whiteboard::Value& value, const whiteboard::ParameterList& rParameters)
{
    switch(resourceId.localResourceId)
    {
        case WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE_CHARHANDLE::LID:
        {
            WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE_CHARHANDLE::SUBSCRIBE::ParameterListRef parameterRef(rParameters);
            if (parameterRef.getCharHandle() == mIntervalCharHandle)
            {
                const WB_RES::Characteristic &charValue = value.convertTo<const WB_RES::Characteristic &>();
                uint16_t interval = *reinterpret_cast<const uint16_t*>(&charValue.bytes[0]);
                DEBUGLOG("onNotify: mIntervalCharHandle: len: %d, new interval: %d", charValue.bytes.size(), interval);
                // Update the interval
                if (interval >= 1 && interval <= 65535)
                    mMeasIntervalSecs = interval;
                // restart timer if exists
                if (mMeasurementTimer != whiteboard::ID_INVALID_TIMER) {
                    stopTimer(mMeasurementTimer);
                    mMeasurementTimer = startTimer(mMeasIntervalSecs*1000, true);
                }
            }
            else if (parameterRef.getCharHandle() == mMeasCharHandle)
            {
                const WB_RES::Characteristic &charValue = value.convertTo<const WB_RES::Characteristic &>();
                bool bNotificationsEnabled = charValue.notifications.hasValue() ? charValue.notifications.getValue() : false;
                DEBUGLOG("onNotify: mMeasCharHandle. bNotificationsEnabled: %d", bNotificationsEnabled);
                // Start or stop the timer
                if (mMeasurementTimer != whiteboard::ID_INVALID_TIMER)
                {
                    stopTimer(mMeasurementTimer);
                    mMeasurementTimer = whiteboard::ID_INVALID_TIMER;
                }
                if (bNotificationsEnabled)
                {
                    //Subscribe to ECG Service
                    int32_t sampleRate = 125;
                    asyncSubscribe(WB_RES::LOCAL::MEAS_ECG_REQUIREDSAMPLERATE(), AsyncRequestOptions::Empty, sampleRate);
                    //Subscribe to Heart Rate Service
                    asyncSubscribe(WB_RES::LOCAL::MEAS_HR());
                    //Start Timer
                    mMeasurementTimer = startTimer(mMeasIntervalSecs*1000, true);
                }
            }
        }
            break;
        case WB_RES::LOCAL::MEAS_ECG_REQUIREDSAMPLERATE::LID:
        {
            WB_RES::ECGData ecgvalue = value.convertTo<WB_RES::ECGData>();
            int32_t ecgarray[16];
            uint32_t ecgtimestamp = ecgvalue.timestamp;
            //Copy ECG Samples
            for(int i = 0; i < 16; i++)
            {
                ecgarray[i] = 0; //initialize to 0
                ecgarray[i] = ecgvalue.samples[i];
            }
            
            //Create Buffer
            uint8_t buffer[36]; // 1 byte or flags, 4 for FLOAT "in Celsius" value
            
            //Initialize buffer
            for(int k = 0; k<38; k++)
            {
                buffer[k] = 0;
            }
            
            buffer[0]=1; //1 byte to notify ECG Data
            floatToFLOAT4(ecgtimestamp, &buffer[1]);
            for(int i = 0, j = 5; i < 16; i++,j=j+2)
            {
                floatToFLOAT2(&ecgarray[i], &buffer[j]);
            }
            buffer[35] = 10; //Stop byte
            // Write the result to measChar. This results NOTIFY to be triggered in GATT service
            WB_RES::Characteristic newMeasCharValue;
            newMeasCharValue.bytes = whiteboard::MakeArray<uint8_t>(buffer, sizeof(buffer));
            asyncPut(WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE_CHARHANDLE(), AsyncRequestOptions::Empty, mTemperatureSvcHandle, mMeasCharHandle, newMeasCharValue);
        }
            break;
            
        case WB_RES::LOCAL::MEAS_HR::LID:
        {
            const WB_RES::HRData& hrdata = value.convertTo<const WB_RES::HRData&>();
            uint16_t hr = hrdata.average;
            uint16_t rrinterval = (uint16_t)hrdata.rrData[0];
            
            // Create Buffer
            uint8_t buffer[6]; // 1 byte or flags, 4 for FLOAT "in Celsius" value
            
            
            //Initialize buffer
            for(int k = 0; k<6; k++)
            {
                buffer[k] = 0;
            }
            buffer[0]=2; //1 byte to notify HR data
            floatToFLOAT1(hr, &buffer[1]);
            floatToFLOAT1(rrinterval, &buffer[3]);
            buffer[5] = 10; //end bit
            // Write the result to measChar. This results NOTIFY to be triggered in GATT service
            WB_RES::Characteristic newMeasCharValue;
            newMeasCharValue.bytes = whiteboard::MakeArray<uint8_t>(buffer, sizeof(buffer));
            asyncPut(WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE_CHARHANDLE(), AsyncRequestOptions::Empty, mTemperatureSvcHandle, mMeasCharHandle, newMeasCharValue);
        }
            break;
    }
}

void CustomGATTSvcClient::onPostResult(whiteboard::RequestId requestId,
                                       whiteboard::ResourceId resourceId,
                                       whiteboard::Result resultCode,
                                       const whiteboard::Value& rResultData) {
    DEBUGLOG("CustomGATTSvcClient::onPostResult: %d", resultCode);
    
    if (resultCode == whiteboard::HTTP_CODE_CREATED) {
        // Custom Gatt service was created
        mTemperatureSvcHandle = (int32_t)rResultData.convertTo<uint16_t>();
        DEBUGLOG("Custom Gatt service was created. handle: %d", mTemperatureSvcHandle);
        
        // Request more info about created svc so we get the char handles
        asyncGet(WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE(), AsyncRequestOptions::Empty, mTemperatureSvcHandle);
    }
}

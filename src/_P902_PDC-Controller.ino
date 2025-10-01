#include "_Plugin_Helper.h"
//#ifdef USES_P902

// #######################################################################################################
// ################Plugin 902: Controller per Pompa di Calore Panasonic Aquarea wh-mdc09c3e5 #############
// #######################################################################################################

/*
   Plugin is based upon SDS011 dust sensor PM2.5 and PM10 lib
   This plugin and lib was written by Jochen Krapf (jk@nerd2nerd.org)

   This plugin reads the particle concentration from SDS011 Sensor
   DevicePin1 - RX on ESP, TX on SDS
   DevicePin2 - TX on ESP, RX on SDS, optional, for setting the sleep time
 */

/** Changelog:
 * 2025-08-05 tonhuisman: Introduce multi-instance use
 * 2025-01-12 tonhuisman: Add support for MQTT AutoDiscovery
 */

# include <ESPeasySerial.h>
# include <GPIO_Direct_Access.h>

# define PLUGIN_902
# define PLUGIN_ID_902         902
# define PLUGIN_NAME_902       "PDC Controller Panasonic Aquarea"
# define PLUGIN_VALUENAME1_902 "MODE" // CDZ MODE (3:heat; 5:cool;21:quiet;4:off)
# define PLUGIN_VALUENAME2_902 "ND1"  // NOT DEFINED
# define PLUGIN_VALUENAME3_902 "T_ESTERNA"  // 
# define PLUGIN_VALUENAME4_902 "T_MANDATA"  // 



uint8_t *Plugin_902_DMXBuffer    = 0;
int16_t  Plugin_902_DMXSize      = 32;
ESPeasySerial *Plugin_902_Serial = nullptr;


boolean Plugin_902(uint8_t function, struct EventStruct *event, String& string)
{
  bool success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
    {
      auto& dev = Device[++deviceCount];
      dev.Number         = PLUGIN_ID_902;
      dev.Type           = DEVICE_TYPE_SERIAL;
      dev.VType          = Sensor_VType::SENSOR_TYPE_QUAD;
      dev.FormulaOption  = true;
      dev.ValueCount     = 4;
      dev.SendDataOption = true;
      dev.TimerOption    = true;
      dev.PluginStats    = true;
      break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
      string = F(PLUGIN_NAME_902);
      break;
    }

    case PLUGIN_SET_DEFAULTS:
    {
      # ifdef ESP8266
      CONFIG_PORT = static_cast<int>(ESPEasySerialPort::serial1); // Serial1 port
      CONFIG_PIN1 = -1;                                           // RX pin
      CONFIG_PIN2 = 2;                                            // TX pin
      # endif // ifdef ESP8266

      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_902));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_902));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_902));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[3], PSTR(PLUGIN_VALUENAME4_902));
      break;
    }

    /*
    # if FEATURE_MQTT_DISCOVER
    case PLUGIN_GET_DISCOVERY_VTYPES:
    {
      event->Par1 = static_cast<int>(Sensor_VType::SENSOR_TYPE_STRING);
      event->Par2 = static_cast<int>(Sensor_VType::SENSOR_TYPE_DUSTPM10_ONLY);
      success     = true;
      break;
    }
    # endif // if FEATURE_MQTT_DISCOVER
    */

    case PLUGIN_GET_DEVICEGPIONAMES:
    {
      serialHelper_getGpioNames(event, false, true); // TX optional
      break;
    }

    case PLUGIN_WEBFORM_SHOW_CONFIG:
    {
      string += serialHelper_getSerialTypeLabel(event);
      success = true;
      break;
    }

    case PLUGIN_WEBFORM_LOAD:
    {
      // FIXME TD-er:  Whether TX pin is connected should be set somewhere
      if (validGpio(CONFIG_PIN2)) {
        addFormNumericBox(F("Sleep time"), F("sleeptime"),
                          PCONFIG(0),
                          0, 30);
        addUnit(F("Minutes"));
        addFormNote(F("0 = continous, 1..30 = Work 30 seconds and sleep n*60-30 seconds"));
      }
      break;
    }
    case PLUGIN_WEBFORM_SAVE:
    {
      if (validGpio(CONFIG_PIN2)) {
        // Communications to device should work.
        const int newsleeptime = getFormItemInt(F("sleeptime"));

        if (PCONFIG(0) != newsleeptime) {
          PCONFIG(0) = newsleeptime;
          Plugin_902_setWorkingPeriod(event, newsleeptime);
        }
      }
      success = true;
      break;
    }

    case PLUGIN_INIT:
    {
      Plugin_902_DMXSize = PCONFIG(0);

      if (Plugin_902_DMXBuffer) {
        delete[] Plugin_902_DMXBuffer;
      }
      Plugin_902_DMXBuffer = new (std::nothrow) uint8_t[Plugin_902_DMXSize];

      if (Plugin_902_DMXBuffer != nullptr) {
        memset(Plugin_902_DMXBuffer, 0, Plugin_902_DMXSize);
      }

      if ((-1 == CONFIG_PIN2) && (2 == CONFIG_PIN1)) { // Convert previous GPIO settings
        CONFIG_PIN2 = CONFIG_PIN1;
        CONFIG_PIN1 = -1;
      }
      int rxPin                    = CONFIG_PIN1;
      int txPin                    = CONFIG_PIN2;
      const ESPEasySerialPort port = static_cast<ESPEasySerialPort>(CONFIG_PORT);

      if ((rxPin < 0) && (txPin < 0)) {
        ESPeasySerialType::getSerialTypePins(port, rxPin, txPin);
        CONFIG_PIN1 = rxPin;
        CONFIG_PIN2 = txPin;
      }
      delete Plugin_902_Serial;
      Plugin_902_Serial = new (std::nothrow) ESPeasySerial(port, rxPin, txPin);

      if (nullptr != Plugin_902_Serial) {
        # ifdef ESP8266
        Plugin_902_Serial->begin(250000, (SerialConfig)SERIAL_8N2);
        # endif // ifdef ESP8266
        # ifdef ESP32
        Plugin_902_Serial->begin(250000, SERIAL_8N2);
        # endif // ifdef ESP32
      }

      success = Plugin_902_DMXBuffer != nullptr && Plugin_902_Serial != nullptr && validGpio(CONFIG_PIN2);
      break;
    }

    case PLUGIN_EXIT:
    {
      break;
    }

    case PLUGIN_FIFTY_PER_SECOND:
    {
      P902_data_struct *P902_data =
        static_cast<P902_data_struct *>(getPluginTaskData(event->TaskIndex));

      if ((nullptr == P902_data) || !P902_data->isInitialized()) {
        break;
      }

      P902_data->Process();

      if (P902_data->available())
      {
        const float pm2_5 = P902_data->GetPM2_5();
        const float pm10  = P902_data->GetPM10_();
        # ifndef BUILD_NO_DEBUG

        if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
          addLog(LOG_LEVEL_DEBUG, strformat(F("SDS  : act %.2f %.2f"), pm2_5, pm10));
        }
        # endif // ifndef BUILD_NO_DEBUG

        if (Settings.TaskDeviceTimer[event->TaskIndex] == 0)
        {
          UserVar.setFloat(event->TaskIndex, 0, pm2_5);
          UserVar.setFloat(event->TaskIndex, 1, pm10);
          event->sensorType = Sensor_VType::SENSOR_TYPE_DUAL;
          sendData(event);
        }
      }

      success = true;
      break;
    }

    case PLUGIN_READ:
    {
      P902_data_struct *P902_data =
        static_cast<P902_data_struct *>(getPluginTaskData(event->TaskIndex));

      if ((nullptr == P902_data) || !P902_data->isInitialized()) {
        break;
      }

      float pm25{};
      float pm10{};

      if (P902_data->ReadAverage(pm25, pm10)) {
        UserVar.setFloat(event->TaskIndex, 0, pm25);
        UserVar.setFloat(event->TaskIndex, 1, pm10);
        success = true;
      }
      break;
    }
  }

  return success;
}

String Plugin_902_ErrorToString(int error) {
  String log;

  if (error < 0) {
    log =  concat(F("comm error: "), error);
  }
  return log;
}

String Plugin_902_WorkingPeriodToString(int workingPeriod) {
  if (workingPeriod < 0) {
    return Plugin_902_ErrorToString(workingPeriod);
  }
  String log;

  if (workingPeriod > 0) {
    log = strformat(F("%d minutes"), workingPeriod);
  } else {
    log = F(" continuous");
  }
  return log;
}

void Plugin_902_setWorkingPeriod(EventStruct *event, int minutes) {
  P902_data_struct *P902_data =
    static_cast<P902_data_struct *>(getPluginTaskData(event->TaskIndex));

  if ((nullptr == P902_data) || !P902_data->isInitialized()) {
    return;
  }
  P902_data->SetWorkingPeriod(minutes);

  if (loglevelActiveFor(LOG_LEVEL_INFO)) {
    addLog(LOG_LEVEL_INFO, concat(F("SDS  : Working Period set to: "), Plugin_902_WorkingPeriodToString(minutes)));
  }
}

// #endif
#endif // USES_P902

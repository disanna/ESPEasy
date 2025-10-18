#include "_Plugin_Helper.h"
#ifdef USES_P202
  
// #######################################################################################################
// ################Plugin 202: Controller per Pompa di Calore Panasonic Aquarea wh-mdc09c3e5 #############
// #######################################################################################################

# define PLUGIN_202
# define PLUGIN_ID_202         202
# define PLUGIN_NAME_202       "PDC Controller Panasonic Aquarea"
# define PLUGIN_VALUENAME1_202 "MODE" // CDZ MODE (3:heat; 5:cool;21:quiet;4:off)
# define PLUGIN_VALUENAME2_202 "ND1"  // NOT DEFINED
# define PLUGIN_VALUENAME3_202 "T_ESTERNA"  // 
# define PLUGIN_VALUENAME4_202 "T_MANDATA"  // 

# define P202_BAUDRATE                   PCONFIG_LONG(0)
# define P202_BAUDRATE_LABEL             PCONFIG_LABEL(0)
# define P202_SERIAL_MODE                PCONFIG(1)
# define P202_SERIAL_MODE_LABEL          PCONFIG_LABEL(1)
# define P202_BYTE_TEST_WRITE_01            PCONFIG(2)
# define P202_BYTE_TEST_WRITE_01_LABEL      PCONFIG_LABEL(2)
# define P202_BYTE_TEST_WRITE_02            PCONFIG(3)
# define P202_BYTE_TEST_WRITE_02_LABEL      PCONFIG_LABEL(3)
# define P202_BYTE_TEST_WRITE_03            PCONFIG(4)
# define P202_BYTE_TEST_WRITE_03_LABEL      PCONFIG_LABEL(4)
# define P202_BYTE_TEST_WRITE_04            PCONFIG(5)
# define P202_BYTE_TEST_WRITE_04_LABEL      PCONFIG_LABEL(5)

ESPeasySerial *Plugin_202_ESPEasySerial = nullptr;

// Forward declarations
void P202_initSerial(int baudRate, int modeIndex);
void P202_testWrite(uint8_t b[], u_int8_t count);
uint8_t calcChecksum(const void *data, size_t len);

boolean Plugin_202(uint8_t function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
    {
      auto& dev = Device[++deviceCount];
      dev.Number         = PLUGIN_ID_202;
      dev.Type           = DEVICE_TYPE_SERIAL;
      dev.VType          = Sensor_VType::SENSOR_TYPE_INT32_QUAD;
      dev.ValueCount     = 4;
      dev.SendDataOption = true;
      dev.TimerOption    = true;
      dev.TimerOptional  = true;
      dev.PluginStats    = true;
      dev.MqttStateClass = true;
      break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
      string = F(PLUGIN_NAME_202);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {
      // FIXME TD-er: Copy names as done in P026_Sysinfo.ino.
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_202));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_202));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_202));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[3], PSTR(PLUGIN_VALUENAME4_202));      
      break;
    }

    case PLUGIN_WEBFORM_SHOW_SERIAL_PARAMS:
    {
      // Called to show optinal extra UART parameters in the web interface (only called for SERIAL devices)
      addFormNumericBox(F("Baudrate"), P202_BAUDRATE_LABEL, P202_BAUDRATE, 100, 115200);
      addUnit(F("baud"));

      const __FlashStringHelper *options[] = {
        F("8N1"),
        F("8E1"),
        F("8O1")
      };

      constexpr size_t optionCount = NR_ELEMENTS(options);
      const FormSelectorOptions selector(optionCount, options);
      selector.addFormSelector(F("Serial Mode"), P202_SERIAL_MODE_LABEL, P202_SERIAL_MODE);

      break;
    }

    # if FEATURE_MQTT_DISCOVER || FEATURE_CUSTOM_TASKVAR_VTYPE
    case PLUGIN_GET_DISCOVERY_VTYPES:
    {
      #  if FEATURE_CUSTOM_TASKVAR_VTYPE

      for (uint8_t i = 0; i < event->Par5; ++i) {
        event->ParN[i] = ExtraTaskSettings.getTaskVarCustomVType(i);  // Custom/User selection
      }
      #  else // if FEATURE_CUSTOM_TASKVAR_VTYPE
      event->Par1 = static_cast<int>(Sensor_VType::SENSOR_TYPE_NONE); // Not yet supported
      #  endif // if FEATURE_CUSTOM_TASKVAR_VTYPE
      success = true;
      break;
    }
    # endif // if FEATURE_MQTT_DISCOVER || FEATURE_CUSTOM_TASKVAR_VTYPE


    case PLUGIN_SET_DEFAULTS:
    {
      CONFIG_PORT = static_cast<int>(ESPEasySerialPort::serial0_swap); // serial0_swap port
      
      CONFIG_PIN1        = 13;                                            
      CONFIG_PIN2        = 15;                                           
      P202_BAUDRATE      = 980;
      P202_SERIAL_MODE = 0;

      success = true; 
      break;
    }

    case PLUGIN_WEBFORM_LOAD:
    {
      // this case defines what should be displayed on the web form, when this plugin is selected
      // The user's selection will be stored in
      // PCONFIG(x) (custom configuration)

      // Make sure not to append data to the string variable in this PLUGIN_WEBFORM_LOAD call.
      // This has changed, so now use the appropriate functions to write directly to the Streaming
      // web_server. This takes much less memory and is faster.
      // There will be an error in the web interface if something is added to the "string" variable.

      // Use any of the following (defined at web_server.ino):
      // addFormNote(F("not editable text added here"));
      // To add some html, which cannot be done in the existing functions, add it in the following way:
      //addRowLabel(F("Analog Pin"));


      // For strings, always use the F() macro, which stores the string in flash, not in memory.

      // const __FlashStringHelper dropdownList[] = { F("option1"), F("option2"), F("option3"), F("option4")};
      // const int dropdownOptions[] = { 1, 2, 3, 4 };
      // constexpr int dropdownCount = NR_ELEMENTS(dropdownOptions);
      // addFormSelector(string, F("drop-down menu"), F("dsptype"), dropdownCount, dropdownList, dropdownOptions, PCONFIG(0));

      // number selection (min_value - max_value)
      addFormNumericBox(F("BYTE DI TEST 01 (dec)"),P202_BYTE_TEST_WRITE_01_LABEL, P202_BYTE_TEST_WRITE_01, 0,255);
      addFormNumericBox(F("BYTE DI TEST 02 (dec)"),P202_BYTE_TEST_WRITE_02_LABEL, P202_BYTE_TEST_WRITE_02, 0,255);
      addFormNumericBox(F("BYTE DI TEST 03 (dec)"),P202_BYTE_TEST_WRITE_03_LABEL, P202_BYTE_TEST_WRITE_03, 0,255);
      addFormNumericBox(F("BYTE DI TEST 04 (dec)"),P202_BYTE_TEST_WRITE_04_LABEL, P202_BYTE_TEST_WRITE_04, 0,255);

      // If custom tasksettings need to be loaded and displayed, this is the place to add that

      // after the form has been loaded, set success and break
      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE:
    {
      // this case defines the code to be executed when the form is submitted
      // the plugin settings should be saved to PCONFIG(x)
      // PCONFIG(0) = getFormItemInt(F("dsptype"));
      // pin configuration will be read from CONFIG_PIN1 and stored
      // If custom tasksettings need to be stored, then here is the place to add that

      // after the form has been saved successfuly, set success and break
      P202_BYTE_TEST_WRITE_01 = getFormItemInt(P202_BYTE_TEST_WRITE_01_LABEL);
      P202_BYTE_TEST_WRITE_02 = getFormItemInt(P202_BYTE_TEST_WRITE_02_LABEL);
      P202_BYTE_TEST_WRITE_03 = getFormItemInt(P202_BYTE_TEST_WRITE_03_LABEL);
      P202_BYTE_TEST_WRITE_04 = getFormItemInt(P202_BYTE_TEST_WRITE_04_LABEL);

      P202_BAUDRATE = getFormItemInt(P202_BAUDRATE_LABEL);
      P202_SERIAL_MODE = getFormItemInt(P202_SERIAL_MODE_LABEL);

      P202_initSerial(P202_BAUDRATE, P202_SERIAL_MODE); //reinit serial ccomm after reading serial parameters
      
      success = true;
      break;
    }

    case PLUGIN_INIT:
    {
      if (Plugin_202_ESPEasySerial != nullptr) {
        delete Plugin_202_ESPEasySerial;
        Plugin_202_ESPEasySerial = nullptr;
      }
      Plugin_202_ESPEasySerial = new (std::nothrow) ESPeasySerial(static_cast<ESPEasySerialPort>(CONFIG_PORT), CONFIG_PIN1, CONFIG_PIN2, true);

      if (Plugin_202_ESPEasySerial == nullptr) {
        break;
      }

      P202_initSerial(P202_BAUDRATE, P202_SERIAL_MODE); //init serial comm 

      success = true;
      break;
    }

    case PLUGIN_READ:
    {
      uint8_t  b;

      while(Plugin_202_ESPEasySerial->available() > 0) {
        b = Plugin_202_ESPEasySerial->read();
        addLogMove(LOG_LEVEL_DEBUG, concat(F("P202: read test = "), b));
      }
      
      success = true;
      break;
    }
  

    case PLUGIN_ONCE_A_SECOND:
    {
      // code to be executed once a second. Tasks which do not require fast response can be added here
      uint8_t testCommandWrite[] = {P202_BYTE_TEST_WRITE_01, 
                                    P202_BYTE_TEST_WRITE_02, 
                                    P202_BYTE_TEST_WRITE_03};

      uint8_t count = sizeof(testCommandWrite)/sizeof(testCommandWrite[0]);
      P202_testWrite(testCommandWrite, count);

      success = true;
    }

    case PLUGIN_TEN_PER_SECOND:
    {
      // code to be executed 10 times per second. Tasks which require fast response can be added here
      // be careful on what is added here. Heavy processing will result in slowing the module down!

      success = true;
    }
  }
  return success;
}

void P202_testWrite(uint8_t b[], u_int8_t count) {

  uint8_t byteCheckSum = calcChecksum(b, count);      
  addLogMove(LOG_LEVEL_DEBUG, concat(F("P202: byteCheckSum ="), byteCheckSum));

  for(int i=0; i< count; i++) {
    Plugin_202_ESPEasySerial->write(b[i]);
  }
  Plugin_202_ESPEasySerial->write(byteCheckSum);
  Plugin_202_ESPEasySerial->flush();
}

void P202_initSerial(int baudRate, int modeIndex) {
  SerialConfig serialMode = SERIAL_8N1;

  switch(modeIndex) {
    case 0:
      serialMode = SERIAL_8N1;
      break;
    case 1:
      serialMode = SERIAL_8E1;
      break;
    case 2:
      serialMode = SERIAL_8O1;
      break;
  }

  Plugin_202_ESPEasySerial->begin(baudRate, serialMode);
  delay(100); //wait for serial to stabilise
}

uint8_t calcChecksum(const void *data, size_t len) {
    const uint8_t *p = static_cast<const uint8_t *>(data);
    uint8_t ret = 0;

    for(size_t i = 0; i < len; i++) {
     ret += p[i];
   }

    if(ret < 256)
      return ret;
    else
      return (ret-256);
}

#endif // USES_P202
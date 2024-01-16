// Feature flags
//#define DEVELOPMENT_MODE

#include <iot_core.h>

namespace io {
// Switches
#ifdef DEVELOPMENT_MODE
gpiobj::DigitalInput otaEnablePin {true};
gpiobj::DigitalInput debugModePin {true};
#else
gpiobj::DigitalInput otaEnablePin { gpiobj::gpios::esp8266::nodemcu::D2, gpiobj::InputMode::PullUp, gpiobj::SignalMode::Inverted };
gpiobj::DigitalInput debugModePin { gpiobj::gpios::esp8266::nodemcu::D7, gpiobj::InputMode::PullUp, gpiobj::SignalMode::Inverted };
#endif
// Buttons
gpiobj::DigitalInput updatePin { gpiobj::gpios::esp8266::nodemcu::D3, gpiobj::InputMode::PullUp, gpiobj::SignalMode::Inverted };
gpiobj::DigitalInput factoryResetPin { gpiobj::gpios::esp8266::nodemcu::D8, gpiobj::InputMode::Normal, gpiobj::SignalMode::Normal };

// Control lines
gpiobj::DigitalOutput builtinLed { LED_BUILTIN, false, gpiobj::SignalMode::Inverted };
}

iot_core::System sys { "iot-core-example", "1.0.0", "ota password", io::builtinLed, io::otaEnablePin, io::updatePin, io::factoryResetPin, io::debugModePin };

void setup() {
  sys.logger().log("ios", iot_core::format(F("ota=%u write=%u tx=%u debug=%u"),
    io::otaEnablePin.read(),
    io::writeEnablePin.read(),
    io::txEnablePin.read(),
    io::debugModePin.read()
  ));

  // Add components here
  //sys.addComponent(&component);

  sys.setup();
}

void loop() {
  sys.loop();
}

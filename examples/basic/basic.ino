#include <iot_core.h>
#include <iot_core/api/Server.h>
#include <iot_core/api/SystemApi.h>

const iot_core::VersionInfo VERSION { "", "1.0.0" };

namespace io {
// These are the I/Os the core uses, configure/map them to any GPIO as needed.

// Switches
gpiobj::DigitalInput otaEnablePin {true};
gpiobj::DigitalInput debugModePin {true};

// Buttons
gpiobj::DigitalInput updatePin {};
gpiobj::DigitalInput factoryResetPin {};

// Control lines
gpiobj::DigitalOutput builtinLed { LED_BUILTIN, false, gpiobj::SignalMode::Inverted };
}

iot_core::System sys { "basic-example", VERSION, "ota password", io::builtinLed, io::otaEnablePin, io::updatePin, io::factoryResetPin, io::debugModePin };
iot_core::api::Server api { sys };
iot_core::api::SystemApi systemApi { sys, sys };

void setup() {
  sys.addComponent(&api);
  api.addProvider(&systemApi);

  sys.setup();
}

void loop() {
  sys.loop();
}

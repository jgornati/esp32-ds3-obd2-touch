#pragma once

#include <stdint.h>

#include <driver/i2c_master.h>

// El I2C de la placa (SDA 47 / SCL 48) y el expansor TCA9554, que usan la
// pantalla (en la V2, el reset y el encendido de la luz) y el sonido; más
// adelante, el IMU y el RTC. Se inicia una vez, antes que la pantalla.
//
// Del TCA9554 se tocan solo los pines que se piden. Los demás quedan como
// entradas, como arrancan: el 6 sostendría la placa con la batería (SYS_EN).

// false si el bus o el expansor no anduvieron; lo que falló sale en el
// monitor serie
bool iniciarPlaca();
// El bus, para agregarle otros dispositivos. nullptr si no se pudo iniciar.
i2c_master_bus_handle_t busPlaca();
// Un pin del TCA9554 como salida, con ese nivel. Primero el nivel y después
// la dirección: así no pasa por el otro. false si el expansor no contesta.
bool salidaExpansor(uint8_t pin, bool nivel);

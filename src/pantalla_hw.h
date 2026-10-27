#pragma once

#include <stdint.h>

// Hardware de la pantalla: panel AXS15231B por QSPI, touch por I2C,
// retroiluminación y el puerto de LVGL. En la V2 de la placa, el reset del
// panel y el encendido de la luz van por el TCA9554: iniciarPlaca() antes. LVGL dibuja en
// horizontal (640x172) y la imagen se gira por software al mandarla al panel (172x640).
//
// LVGL es de loop(): atender() llama a lv_timer_handler(). Lo que bloquea
// sin llamar a atender() (conectar el BLE: hasta 30 s si el adaptador no
// contesta) va entre soltar() y tomar(), y mientras tanto otra tarea atiende
// LVGL: las animaciones siguen y el touch se lee (mantener apretado para
// reiniciar anda aunque loop() esté trabado). Lo que use LVGL desde adentro
// de eso (los avisos de la conexión) va entre tomar() y soltar().
class PantallaHw {
public:
  // Panel, touch y LVGL. Deja la pantalla en negro, con la luz en brillo
  // (de 0 a 255), y LVGL tomado por quien llama.
  bool iniciar(uint8_t brillo);
  bool iniciada() const;
  // Llamar seguido desde loop() y mientras se espera al ELM. El touch se lee
  // solo acá.
  void atender();
  // Se pueden anidar (es un mutex recursivo). iniciar() lo deja tomado una vez.
  void soltar();
  void tomar();
  // Lo más que pasó entre dos atender() desde la última llamada: un toque
  // más corto que eso se puede perder
  uint32_t huecoMaximo();
  // Dibuja y manda ya lo que cambió, sin esperar el período de LVGL. Para
  // antes de algo que bloquea sin llamar a atender().
  void dibujarYa();

  // Dibuja y manda la pantalla entera. Devuelve los ms del refresco
  // completo; envio, los de mandarla al panel.
  uint32_t medirRefresco(uint32_t &envio);

  // Última lectura del touch, sin girar (para calibrar). false sin dedo.
  bool toqueCrudo(uint16_t &x, uint16_t &y) const;

  // De 0 (apagada) a 255 (al máximo)
  void ponerBrillo(uint8_t nivel);
  uint8_t brillo() const;
};

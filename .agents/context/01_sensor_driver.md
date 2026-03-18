# Tarea: Driver del Sensor y Adquisición I2C

## Objetivo
Implementar una capa de abstracción C++ para el sensor MLX90640 que garantice estabilidad en el bus I2C y entrega determinista de frames.

## Requisitos Técnicos
1. **Capa OO**: Clase `MLX90640_Sensor` que oculte las llamadas a la API de Melexis.
2. **Bus I2C**: Configurar a 400kHz (Fast Mode) para mayor estabilidad ante ruidos. 
3. **DMA/Interrupts**: Si es posible, usar el driver avanzado de ESP-IDF para evitar bloqueos del Core 1.
4. **Resilencia**: Si el sensor falla en el arranque, NO debe bloquear el resto del sistema (WiFi/Web). Debe intentar re-inicializarse o mostrar error en el HUD.

## Referencias
- `lib/mlx90640-library-master/`
- `sdkconfig.defaults` (Configuración de I2C)

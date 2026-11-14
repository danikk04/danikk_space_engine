# Сценарии

## `scenario/base.h`

- `startBaseScenario(World&)` — создаёт стартовое содержимое мира.

Реализация `source/scenario/base.cpp` создаёт WorldBody планеты с `PlanetComponent`, начальную позицию камеры и компоненты камеры/локального контроллера. Сценарий вызывается из `World::init()` после запуска `ChunkLoader`.

Сценарии отделены от механики `World`: они собирают начальное состояние, используя API `WorldData`.

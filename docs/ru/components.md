# Система компонентов

Компоненты хранятся в слотах `WorldData`, а таблицы типов предоставляют функции жизненного цикла и обновления. Слот содержит ID владельца, тип и payload: inline до 32 байт или отдельно выделенная память.

## `component/point/point.h`

- `PointComponentTypeId` — типы `camera`, `local_player_controller_component`, `machine_gun`, `bullet` и `engine`.
- `PointComponentType` — размер/выравнивание данных и callbacks `construct`, `move`, `destroy`, `tick`, `frame`.
- `PointComponentSlot` — привязка к `PointId`, `ChunkId`, типу и области хранения payload; `dispose()` возвращает слот в исходное состояние.
- `getPointComponentType()` возвращает описание типа.

## `component/graph/graph.h`

- `GraphComponentSlot` связывает `point_a` и `point_b`, хранит тип и payload.
- `GraphComponentType` задаёт callbacks жизненного цикла, `tick` и `frame`; зарегистрированы типы pipe и wire.
- Слоты граф-компонентов принадлежат пулу `WorldData` и регистрируются в чанке.

## `component/world_body/world_body.h`

- `WorldBodyComponentType` задаёт callbacks тела и callback `accept_generated_chunk`.
- `WorldBodyComponentSlot` привязан к `WorldBodyId` и содержит generation ID, который позволяет не принимать результаты от уже заменённого компонента.
- Текущий компонент тела мира — `PlanetComponent`; см. [Planet](planet.md).

## Компоненты точки

- `component/point/camera.h` — ориентация, базис камеры, FOV и near/far clip. `tickCameraComponent()` обрабатывает движение и мышь; `frameCameraComponent()` задаёт проекцию.
- `component/point/local_player_controller.h` — активный инструмент, выбранный объект, скролл-аккумуляторы и массив `selected_faces`. В `tick()` выполняются raycast, ввод и применение инструмента.
- `component/point/machinegun.h` — ориентация модели и таймер стрельбы; tick создаёт пули, frame рисует модель.
- `component/point/bullet.h` — скорость, возраст, время жизни и состояние отложенного удаления; tick двигает пулю и проверяет столкновения, frame отображает её след.

## Владение и обновление

- Компонент точки добавляется в конкретный чанк через `WorldData::addPointComponentSlotToChunk()`; чанк вызывает его `tick` и `frame` callback.
- Граф-компонент ссылается на две точки и хранится в списке граф-компонентов чанка.
- WorldBody-компоненты обходятся `World::tick()`/`World::frame()` отдельно от компонентов чанков.
- Удаление слота сначала вызывает callback `destroy`, освобождает внешний payload при необходимости, вызывает `dispose()` и возвращает ID в пул.

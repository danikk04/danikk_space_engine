# Инструменты и редактирование

## `tool.h`

- `ToolId` — `none`, `select`, `expand`, `erase_faces`, `create`.
- `CreateObjectId` — `none`, `machinegun`, `volume_cell`, `terrain`.
- `ToolApplyResult` — сообщает, требуется ли заменить список выделений, и содержит новый список.
- `applyTool()` направляет действие к реализации выбранного инструмента.
- `stepTool()` и `stepCreateObject()` циклически меняют выбор.

## Контроллер

`LocalPlayerControllerComponent::tick()` находится в `component/point/local_player_controller.h` и реализации рядом с ним.

- Колёсико меняет инструмент.
- Alt + колёсико меняет объект для инструмента Create.
- ЛКМ применяет активный инструмент.
- `selected_faces[0..selected_face_count)` — закреплённые выделения; дополнительная последняя запись может быть гранью под курсором.
- Для Expand/Erase при наличии выделений используются выделенные грани; иначе используется грань под курсором.

## Реализации инструментов

- `source/tool/select.cpp` переключает выделение грани под курсором.
- `source/tool/expand.cpp` вызывает `World::extrudeFaces()`.
- `source/tool/erase.cpp` вызывает удаление выбранных объёмных ячеек либо добавляет вычитающий terrain stamp для поверхности.
- `source/tool/create.cpp` создаёт пулемёт, случайную объёмную ячейку или добавляющий terrain stamp на поверхности.
- `source/tool/none.cpp` и `source/tool/tool.cpp` содержат поведение пустого инструмента и маршрутизацию.

## Идентификация грани

`FacePtr` различает грань `VolumeCell` и `GroundTriangle`. Подсветка и контроллер хранят именно этот ID; геометрия грани извлекается из текущего чанка при raycast/отрисовке.

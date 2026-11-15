# Формы и коллизии: legacy-заготовки

Эта подсистема не входит в активный runtime планетарного мира.

## `shape/shape.h`

- `ShapeId` — ID формы.
- `ShapeType` — типы `none`, `cube`, `sphere`.
- `ShapeDefinition` — фабрика меша и кэш `ShapeMesh` для размеров.
- `initShapeTable()`, `findShape()` и `clearShapeMeshCache()` управляют таблицей форм и кэшем.

## `shape/collision_matrix.h`

- `CollisionFunction` и `collision_matrix` задают таблицу функций проверки пар форм.
- Текущая реализация связана с прежней `GameObject`-моделью и не подключена к основной сборке.

Перед возвращением подсистемы в runtime нужно согласовать API с текущими `Point`, `GroundTriangle`, `Chunk` и активной системой столкновений.

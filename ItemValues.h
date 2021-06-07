// AWindow private definitions.

#define UTF8(str)   str.toUtf8().constData()

#define GIT_PIXMAP  QGraphicsPixmapItem::Type
#define GIT_RECT    QGraphicsRectItem::Type

// QGraphicsItem::data() key.
enum ItemDataKey {
    ID_NAME
};

struct ItemValues {
    QByteArray name;
    int x, y, w, h;
};

extern void itemValues(ItemValues& iv, const QGraphicsItem* item);

typedef QList<QGraphicsItem*> ItemList;

#define each_item(pi) \
    for(const QGraphicsItem* pi : _scene->items(Qt::AscendingOrder))

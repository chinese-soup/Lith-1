#include "lith.h"

#include "datamodel.h"
#include "weechat.h"

#include <iostream>
#include <QThread>
#include <QEventLoop>
#include <QAbstractEventDispatcher>

Lith *Lith::_self = nullptr;
Lith *Lith::instance() {
    if (!_self)
        _self = new Lith();
    return _self;
}

ProxyBufferList *Lith::buffers() {
    return m_proxyBufferList;
}

QmlObjectList *Lith::unfilteredBuffers() {
    return m_buffers;
}

Buffer *Lith::selectedBuffer() {
    if (m_selectedBufferIndex >=0 && m_selectedBufferIndex < m_buffers->count())
        return m_buffers->get<Buffer>(m_selectedBufferIndex);
    return nullptr;
}

void Lith::selectedBufferSet(Buffer *b) {
    for (int i = 0; i < m_buffers->count(); i++) {
        auto it = m_buffers->get<Buffer>(i);
        if (it && it == b) {
            selectedBufferIndexSet(i);
            return;
        }
    }
    selectedBufferIndexSet(-1);
}

int Lith::selectedBufferIndex() {
    return m_selectedBufferIndex;
}

void Lith::selectedBufferIndexSet(int index) {
    if (m_selectedBufferIndex != index && index < m_buffers->count()) {
        m_selectedBufferIndex = index;
        emit selectedBufferChanged();
        if (selectedBuffer())
            selectedBuffer()->fetchMoreLines();
        if (index >= 0)
            settingsGet()->lastOpenBufferSet(index);
    }
}

QObject *Lith::getObject(pointer_t ptr, const QString &type, pointer_t parent) {
    if (type.isEmpty()) {
        if (m_bufferMap.contains(ptr))
            return m_bufferMap[ptr];
        if (m_lineMap.contains(ptr))
            return m_lineMap[ptr];
        return nullptr;
    }
    else if (type == "line_data") {/*
        if (m_bufferMap.contains(parent)) {
            //Buffer *buffer = m_bufferMap[parent];
            //return buffer->getLine(ptr);
        }
        else if (parent == 0) { */
            if (!m_lineMap.contains(ptr))
                m_lineMap[ptr] = new BufferLine(nullptr);
            return m_lineMap[ptr];
            /*
        }
        else {
            qCritical() << "Got line information before the buffer was allocated";
        }
        */
    }
    else if (type == "nicklist_item") {
        if (m_bufferMap.contains(parent)) {
            return m_bufferMap[parent]->getNick(ptr);
        }
    }
    else if (type == "hotlist") {
        //qCritical() << ptr;
        if (!m_hotList.contains(ptr))
            m_hotList.insert(ptr, new HotListItem(nullptr));
        return m_hotList[ptr];
    }
    else {
        //qCritical() << "Unknown type of new stuff requested:" << type;
    }
    return nullptr;
}

Weechat *Lith::weechat() {
    return m_weechat;
}

Lith::Lith(QObject *parent)
    : QObject(parent)
    , m_weechatThread(new QThread(this))
    , m_weechat(new Weechat(this))
    , m_buffers(QmlObjectList::create<Buffer>())
    , m_proxyBufferList(new ProxyBufferList(this, m_buffers))
{
    connect(settingsGet(), &Settings::passphraseChanged, this, &Lith::hasPassphraseChanged);

    m_weechat->moveToThread(m_weechatThread);
    m_weechatThread->start();
    QTimer::singleShot(1, m_weechat, &Weechat::init);
}

bool Lith::hasPassphrase() const {
    return !settingsGet()->passphraseGet().isEmpty();
}

void Lith::resetData() {
    selectedBufferIndexSet(-1);
    for (int i = 0; i < m_buffers->count(); i++) {
        if (m_buffers->get<Buffer>(i))
            m_buffers->get<Buffer>(i)->deleteLater();
    }
    m_buffers->clear();
    m_bufferMap.clear();
    qCritical() << "=== RESET";
    int lines = 0;
    for (auto i : m_lineMap) {
        if (i) {
            if (!i->parent())
                lines++;
            i->deleteLater();
        }
    }
    m_lineMap.clear();
    qCritical() << "There is" << m_lineMap.count() << "orphan lines";
    int hotlist = 0;
    for (auto i : m_hotList) {
        if (i) {
            if (!i->parent())
                hotlist++;
            i->deleteLater();
        }
    }
    m_hotList.clear();
    qCritical() << "There is" << m_hotList.count() << "hotlist items";
}

void Lith::handleBufferInitialization(Protocol::HData *hda) {
    for (auto i : hda->data) {
        // buffer
        auto ptr = i.pointers.first();
        auto b = new Buffer(this, ptr);
        for (auto j : i.objects.keys()) {
            b->setProperty(qPrintable(j), i.objects[j]);
        }
        addBuffer(ptr, b);
    }
    delete hda;
}

void Lith::handleFirstReceivedLine(Protocol::HData *hda) {
    for (auto i : hda->data) {
        // buffer - lines - line - line_data
        auto bufPtr = i.pointers.first();
        auto linePtr = i.pointers.last();
        auto buffer = getBuffer(bufPtr);
        if (!buffer) {
            qWarning() << "Line missing a parent:";
            continue;
        }
        auto line = getLine(bufPtr, linePtr);
        if (line)
            continue;
        line = new BufferLine(buffer);
        for (auto j : i.objects.keys()) {
            line->setProperty(qPrintable(j), i.objects[j]);
        }
        buffer->appendLine(line);
        addLine(bufPtr, linePtr, line);
    }
    delete hda;
}

void Lith::handleHotlistInitialization(Protocol::HData *hda) {
    for (auto i : hda->data) {
        // hotlist
        auto ptr = i.pointers.first();
        auto item = new HotListItem(this);
        for (auto j : i.objects.keys()) {
            item->setProperty(qPrintable(j), i.objects[j]);
        }
        addHotlist(ptr, item);
    }
    delete hda;
}

void Lith::handleNicklistInitialization(Protocol::HData *hda) {
    for (auto i : hda->data) {
        // buffer - nicklist_item
        auto bufPtr = i.pointers.first();
        auto nickPtr = i.pointers.last();
        auto buffer = getBuffer(bufPtr);
        if (!buffer) {
            qWarning() << "Nick missing a parent:";
            continue;
        }
        auto nick = new Nick(buffer);
        for (auto j : i.objects.keys()) {
            nick->setProperty(qPrintable(j), i.objects[j]);
        }
        buffer->addNick(nickPtr, nick);
    }
    delete hda;
}

void Lith::handleFetchLines(Protocol::HData *hda) {
    for (auto i : hda->data) {
        // buffer - lines - line - line_data
        auto bufPtr = i.pointers.first();
        auto linePtr = i.pointers.last();
        auto buffer = getBuffer(bufPtr);
        if (!buffer) {
            qWarning() << "Line missing a parent:";
            continue;
        }
        auto line = getLine(bufPtr, linePtr);
        if (line)
            continue;
        line = new BufferLine(buffer);
        for (auto j : i.objects.keys()) {
            line->setProperty(qPrintable(j), i.objects[j]);
        }
        buffer->appendLine(line);
        addLine(bufPtr, linePtr, line);
    }
    delete hda;
}

void Lith::handleHotlist(Protocol::HData *hda) {
    qCritical() << __FUNCTION__ << "is not implemented yet";
    delete hda;
}

void Lith::_buffer_opened(Protocol::HData *hda) {
    for (auto i : hda->data) {
        // buffer
        auto bufPtr = i.pointers.first();
        auto buffer = getBuffer(bufPtr);
        if (buffer)
            continue;
        buffer = new Buffer(this, bufPtr);
        for (auto j : i.objects.keys()) {
            buffer->setProperty(qPrintable(j), i.objects[j]);
        }
        addBuffer(bufPtr, buffer);
    }
    delete hda;
}

void Lith::_buffer_type_changed(Protocol::HData *hda) {
    qCritical() << __FUNCTION__ << "is not implemented yet";
    delete hda;
}

void Lith::_buffer_moved(Protocol::HData *hda) {
    qCritical() << __FUNCTION__ << "is not implemented yet";
    delete hda;
}

void Lith::_buffer_merged(Protocol::HData *hda) {
    qCritical() << __FUNCTION__ << "is not implemented yet";
    delete hda;
}

void Lith::_buffer_unmerged(Protocol::HData *hda) {
    qCritical() << __FUNCTION__ << "is not implemented yet";
    delete hda;
}

void Lith::_buffer_hidden(Protocol::HData *hda) {
    qCritical() << __FUNCTION__ << "is not implemented yet";
    delete hda;
}

void Lith::_buffer_unhidden(Protocol::HData *hda) {
    qCritical() << __FUNCTION__ << "is not implemented yet";
    delete hda;
}

void Lith::_buffer_renamed(Protocol::HData *hda) {
    qCritical() << __FUNCTION__ << "is not implemented yet";
    delete hda;
}

void Lith::_buffer_title_changed(Protocol::HData *hda) {
    qCritical() << __FUNCTION__ << "is not implemented yet";
    delete hda;
}

void Lith::_buffer_localvar_added(Protocol::HData *hda) {
    qCritical() << __FUNCTION__ << "is not implemented yet";
    delete hda;
}

void Lith::_buffer_localvar_changed(Protocol::HData *hda) {
    qCritical() << __FUNCTION__ << "is not implemented yet";
    delete hda;
}

void Lith::_buffer_localvar_removed(Protocol::HData *hda) {
    qCritical() << __FUNCTION__ << "is not implemented yet";
    delete hda;
}

void Lith::_buffer_closing(Protocol::HData *hda) {
    for (auto i : hda->data) {
        // buffer
        auto bufPtr = i.pointers.first();
        auto buffer = getBuffer(bufPtr);
        if (!buffer)
            continue;

        buffer->deleteLater();
    }
    delete hda;
}

void Lith::_buffer_cleared(Protocol::HData *hda) {
    qCritical() << __FUNCTION__ << "is not implemented yet";
    delete hda;
}

void Lith::_buffer_line_added(Protocol::HData *hda) {
    for (auto i : hda->data) {
        // line_data
        auto linePtr = i.pointers.last();
        // path doesn't contain the buffer, we need to retrieve it like this
        auto bufPtr = qvariant_cast<pointer_t>(i.objects["buffer"]);
        auto buffer = getBuffer(bufPtr);
        if (!buffer) {
            qWarning() << "Line missing a parent:";
            continue;
        }
        auto line = getLine(bufPtr, linePtr);
        if (line) {
            continue;
        }
        line = new BufferLine(buffer);
        for (auto j : i.objects.keys()) {
            if (j == "buffer")
                continue;
            line->setProperty(qPrintable(j), i.objects[j]);
        }
        buffer->prependLine(line);
        addLine(bufPtr, linePtr, line);
    }
    delete hda;
}

void Lith::_nicklist(Protocol::HData *hda) {
    qCritical() << __FUNCTION__ << "is not implemented yet";
    delete hda;
}

void Lith::_nicklist_diff(Protocol::HData *hda) {
    qCritical() << __FUNCTION__ << "is not implemented yet";
    delete hda;
}

void Lith::addBuffer(pointer_t ptr, Buffer *b) {
    m_bufferMap[ptr] = b;
    m_buffers->append(b);
    auto lastOpenBuffer = settingsGet()->lastOpenBufferGet();
    if (m_buffers->count() == 1 && lastOpenBuffer < 0)
        emit selectedBufferChanged();
    else if (lastOpenBuffer == m_buffers->count() - 1) {
        selectedBufferIndexSet(lastOpenBuffer);
    }
}

void Lith::removeBuffer(pointer_t ptr) {
    if (m_bufferMap.contains(ptr)) {
        auto buf = m_bufferMap[ptr];
        m_bufferMap.remove(ptr);
        m_buffers->removeItem(buf);
    }
}

Buffer *Lith::getBuffer(pointer_t ptr) {
    if (m_bufferMap.contains(ptr))
        return m_bufferMap[ptr];
    return nullptr;
}

void Lith::addLine(pointer_t bufPtr, pointer_t linePtr, BufferLine *line) {
    auto ptr = bufPtr << 32 | linePtr;
    if (m_lineMap.contains(ptr)) {
        // TODO
        qCritical() << "Line with ptr" << QString("%1").arg(ptr, 16, 16, QChar('0')) << "already exists";
        qCritical() << "Original: " << m_lineMap[ptr]->messageGet();
        qCritical() << "New:" << line->messageGet();
    }
    m_lineMap[ptr] = line;
}

BufferLine *Lith::getLine(pointer_t bufPtr, pointer_t linePtr) {
    auto ptr = bufPtr << 32 | linePtr;
    if (m_lineMap.contains(ptr)) {
        return m_lineMap[ptr];
    }
    return nullptr;
}

void Lith::addHotlist(pointer_t ptr, HotListItem *hotlist) {
    if (m_hotList.contains(ptr)) {
        // TODO
        qCritical() << "Hotlist with ptr" << QString("%1").arg(ptr, 8, 16, QChar('0')) << "already exists";
    }
    m_hotList[ptr] = hotlist;
}


ProxyBufferList::ProxyBufferList(QObject *parent, QAbstractListModel *parentModel)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(parentModel);
    setFilterRole(Qt::UserRole);
    connect(this, &ProxyBufferList::filterWordChanged, [this] {
        setFilterFixedString(filterWordGet());
    });
}
bool ProxyBufferList::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    auto index = sourceModel()->index(source_row, 0, source_parent);
    auto v = sourceModel()->data(index);
    auto b = qvariant_cast<Buffer*>(v);
    if (b) {
        return b->nameGet().toLower().contains(filterWordGet().toLower());
    }
    return false;
}


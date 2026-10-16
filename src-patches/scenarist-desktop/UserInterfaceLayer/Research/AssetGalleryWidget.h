#ifndef VN_ASSET_GALLERY_WIDGET_H
#define VN_ASSET_GALLERY_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QUuid>
#include <QImageReader>

// VN CZ: a project-relative image library. No external absolute paths
// are stored, and the SQLite .kitsp database is never modified.
class AssetGalleryWidget : public QWidget
{
public:
    explicit AssetGalleryWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* main = new QVBoxLayout(this);
        main->setContentsMargins(0, 8, 0, 0);
        auto* buttons = new QHBoxLayout;
        auto* title = new QLabel(tr("Visual assets"), this);
        auto* add = new QPushButton(tr("Add images"), this);
        auto* folder = new QPushButton(tr("Open asset folder"), this);
        buttons->addWidget(title);
        buttons->addStretch();
        buttons->addWidget(add);
        buttons->addWidget(folder);
        main->addLayout(buttons);

        m_list = new QListWidget(this);
        m_list->setViewMode(QListView::IconMode);
        m_list->setIconSize(QSize(110, 88));
        m_list->setResizeMode(QListView::Adjust);
        m_list->setSpacing(6);
        m_list->setMinimumHeight(145);
        m_list->setSelectionMode(QAbstractItemView::SingleSelection);
        main->addWidget(m_list);

        auto* edit = new QHBoxLayout;
        edit->addWidget(new QLabel(tr("Status:"), this));
        m_status = new QComboBox(this);
        m_status->addItem(tr("Concept"), "Concept");
        m_status->addItem(tr("Work in progress"), "WIP");
        m_status->addItem(tr("Final"), "Final");
        m_status->addItem(tr("For game"), "Game");
        edit->addWidget(m_status);
        m_primary = new QPushButton(tr("Set as main"), this);
        m_remove = new QPushButton(tr("Remove image"), this);
        edit->addWidget(m_primary);
        edit->addWidget(m_remove);
        edit->addStretch();
        main->addLayout(edit);
        m_preview = new QLabel(this);
        m_preview->setAlignment(Qt::AlignCenter);
        m_preview->setMinimumHeight(100);
        m_preview->setMaximumHeight(220);
        main->addWidget(m_preview);

        connect(add, &QPushButton::clicked, this, [this] { addImages(); });
        connect(folder, &QPushButton::clicked, this, [this] {
            const QString dir = galleryDir();
            if (dir.isEmpty()) return;
            QDir().mkpath(dir);
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        });
        connect(m_list, &QListWidget::currentRowChanged, this, [this](int) { showSelected(); });
        connect(m_status, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
            const int index = m_list->currentRow();
            if (m_loading || index < 0 || index >= m_items.size()) return;
            QJsonObject item = m_items.at(index).toObject();
            item["status"] = m_status->currentData().toString();
            m_items.replace(index, item);
            save();
            const int oldIndex = index;
            refresh();
            m_list->setCurrentRow(oldIndex);
        });
        connect(m_primary, &QPushButton::clicked, this, [this] {
            const int selected = m_list->currentRow();
            if (selected < 0) return;
            for (int i = 0; i < m_items.size(); ++i) {
                QJsonObject item = m_items.at(i).toObject();
                item["primary"] = (i == selected);
                m_items.replace(i, item);
            }
            save();
            refresh();
            m_list->setCurrentRow(selected);
        });
        connect(m_remove, &QPushButton::clicked, this, [this] {
            const int selected = m_list->currentRow();
            if (selected < 0 || selected >= m_items.size()) return;
            if (QMessageBox::question(this, tr("Remove image"),
                    tr("Remove the selected image from the project library?"),
                    QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;
            const QString path = QDir(galleryDir()).filePath(m_items.at(selected).toObject().value("file").toString());
            if (!QFile::remove(path) && QFile::exists(path)) {
                QMessageBox::warning(this, tr("Image error"), tr("Could not remove the image file."));
                return;
            }
            m_items.removeAt(selected);
            if (!m_items.isEmpty()) {
                bool hasMain = false;
                for (const auto& v : m_items) hasMain |= v.toObject().value("primary").toBool();
                if (!hasMain) {
                    QJsonObject first = m_items.at(0).toObject();
                    first["primary"] = true;
                    m_items.replace(0, first);
                }
            }
            save();
            refresh();
        });
        showSelected();
    }

    void setContext(const QString& projectFile, const QString& kind, int id)
    {
        const QString key = projectFile + "|" + kind + "|" + QString::number(id);
        if (key == m_key) return;
        m_key = key;
        m_projectFile = projectFile;
        m_kind = kind;
        m_id = id;
        m_items = QJsonArray();
        if (!galleryDir().isEmpty()) {
            QFile file(QDir(galleryDir()).filePath("gallery.json"));
            if (file.open(QIODevice::ReadOnly)) {
                const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
                if (document.isArray()) m_items = document.array();
            }
        }
        refresh();
    }

private:
    QString galleryDir() const
    {
        if (m_projectFile.isEmpty() || m_id <= 0 || m_kind.isEmpty()) return QString();
        const QFileInfo project(m_projectFile);
        return QDir(project.absolutePath()).filePath(
            project.completeBaseName() + "_assets/" + m_kind + "/" + QString::number(m_id));
    }

    void save()
    {
        const QString dir = galleryDir();
        if (dir.isEmpty() || !QDir().mkpath(dir)) {
            QMessageBox::warning(this, tr("Image error"), tr("Could not create the project asset folder."));
            return;
        }
        QSaveFile file(QDir(dir).filePath("gallery.json"));
        if (!file.open(QIODevice::WriteOnly) ||
                file.write(QJsonDocument(m_items).toJson(QJsonDocument::Indented)) < 0 ||
                !file.commit()) {
            QMessageBox::warning(this, tr("Image error"), tr("Could not save image metadata."));
        }
    }

    void addImages()
    {
        const QString dir = galleryDir();
        if (dir.isEmpty()) {
            QMessageBox::warning(this, tr("Image error"), tr("Save the project before adding images."));
            return;
        }
        const QStringList sources = QFileDialog::getOpenFileNames(this, tr("Add images"), QString(),
            tr("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
        if (sources.isEmpty()) return;
        if (!QDir().mkpath(dir)) {
            QMessageBox::warning(this, tr("Image error"), tr("Could not create the project asset folder."));
            return;
        }
        for (const QString& source : sources) {
            const QFileInfo info(source);
            QImageReader reader(source);
            if (!reader.canRead()) continue;
            const QString name = QUuid::createUuid().toString(QUuid::WithoutBraces)
                + "." + info.suffix().toLower();
            if (!QFile::copy(source, QDir(dir).filePath(name))) {
                QMessageBox::warning(this, tr("Image error"), tr("Could not copy an image into the project."));
                continue;
            }
            QJsonObject item;
            item["file"] = name;
            item["name"] = info.completeBaseName();
            item["status"] = "Concept";
            item["primary"] = m_items.isEmpty();
            m_items.append(item);
        }
        save();
        refresh();
    }

    void refresh()
    {
        m_loading = true;
        m_list->clear();
        const QString dir = galleryDir();
        for (const auto& itemValue : m_items) {
            const QJsonObject item = itemValue.toObject();
            const QString imagePath = QDir(dir).filePath(item.value("file").toString());
            QPixmap image(imagePath);
            QString caption = item.value("name").toString();
            if (item.value("primary").toBool()) caption = QString::fromUtf8("★ ") + caption;
            auto* viewItem = new QListWidgetItem(QIcon(image.scaled(
                QSize(110, 88), Qt::KeepAspectRatio, Qt::SmoothTransformation)), caption);
            viewItem->setToolTip(caption + " (" + item.value("status").toString() + ")");
            m_list->addItem(viewItem);
        }
        m_loading = false;
        if (m_list->count()) m_list->setCurrentRow(0);
        else showSelected();
    }

    void showSelected()
    {
        const int index = m_list->currentRow();
        const bool selected = index >= 0 && index < m_items.size();
        m_status->setEnabled(selected);
        m_remove->setEnabled(selected);
        m_primary->setEnabled(selected);
        m_preview->clear();
        if (!selected) return;
        const QJsonObject item = m_items.at(index).toObject();
        m_loading = true;
        const int statusIndex = m_status->findData(item.value("status").toString());
        m_status->setCurrentIndex(statusIndex >= 0 ? statusIndex : 0);
        m_loading = false;
        const QPixmap image(QDir(galleryDir()).filePath(item.value("file").toString()));
        if (!image.isNull()) m_preview->setPixmap(image.scaled(
            QSize(600, 210), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    QString m_key;
    QString m_projectFile;
    QString m_kind;
    int m_id = 0;
    bool m_loading = false;
    QJsonArray m_items;
    QListWidget* m_list = nullptr;
    QComboBox* m_status = nullptr;
    QPushButton* m_primary = nullptr;
    QPushButton* m_remove = nullptr;
    QLabel* m_preview = nullptr;
};

#endif // VN_ASSET_GALLERY_WIDGET_H

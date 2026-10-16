#!/usr/bin/env python3
"""Apply VN CZ 1.03 asset gallery UI patches to the checked-out upstream source.
Fail closed if upstream code structure changes.
"""
from pathlib import Path
import os

workspace = Path(os.environ["GITHUB_WORKSPACE"])
root = workspace / "upstream/src/bin/scenarist-desktop"
patches = workspace / "inputs/src-patches/scenarist-desktop"

def edit(path, before, after):
    text = path.read_text(encoding="utf-8")
    count = text.count(before)
    if count != 1:
        raise RuntimeError(f"Asset gallery patch failed for {path}: anchor count {count}")
    path.write_text(text.replace(before, after), encoding="utf-8", newline="\n")

header_dir = root / "UserInterfaceLayer/Research"
gallery_header = patches / "UserInterfaceLayer/Research/AssetGalleryWidget.h"
if not gallery_header.exists():
    raise RuntimeError(f"Missing asset gallery header: {gallery_header}")
(header_dir / "AssetGalleryWidget.h").write_bytes(gallery_header.read_bytes())

view_h = header_dir / "ResearchView.h"
edit(view_h,
     "class SimpleTextEditor;\n",
     "class SimpleTextEditor;\nclass AssetGalleryWidget;\n")
edit(view_h,
     "        void editCharactersRoot();",
     """        void setAssetProjectPath(const QString& _file);
        void setAssetIdentity(const QString& _kind, int _id);

        void editCharactersRoot();""")
edit(view_h,
     "        Ui::ResearchView* m_ui = nullptr;",
     """        Ui::ResearchView* m_ui = nullptr;
        AssetGalleryWidget* m_characterAssets = nullptr;
        AssetGalleryWidget* m_locationAssets = nullptr;
        QString m_assetProjectFile;
        QString m_assetKind;
        int m_assetId = 0;""")

view_cpp = header_dir / "ResearchView.cpp"
edit(view_cpp,
     '#include "ResearchView.h"\n',
     '#include "ResearchView.h"\n#include "AssetGalleryWidget.h"\n#include <QSplitter>\n')
edit(view_cpp,
     "    m_ui->setupUi(this);\n\n    initView();",
     """    m_ui->setupUi(this);

    m_characterAssets = new AssetGalleryWidget(m_ui->characterEdit);
    m_locationAssets = new AssetGalleryWidget(m_ui->locationEdit);

    // Place the large preview to the right of the existing description editor.
    // Gallery thumbnails and controls remain below that two-column area.
    auto attachPreview = [](QWidget* panel, QWidget* editor, AssetGalleryWidget* gallery) {
        QBoxLayout* outer = qobject_cast<QBoxLayout*>(panel->layout());
        const int descriptionPosition = outer->indexOf(editor);
        outer->removeWidget(editor);
        auto* split = new QSplitter(Qt::Horizontal, panel);
        split->addWidget(editor);
        split->addWidget(gallery->previewWidget());
        split->setStretchFactor(0, 3);
        split->setStretchFactor(1, 2);
        split->setSizes(QList<int>() << 680 << 430);
        outer->insertWidget(descriptionPosition, split);
        outer->addWidget(gallery);
    };
    attachPreview(m_ui->characterEdit, m_ui->characterDescription, m_characterAssets);
    attachPreview(m_ui->locationEdit, m_ui->locationDescription, m_locationAssets);

    initView();""")
edit(view_cpp,
     "void ResearchView::editCharactersRoot()\n",
     """void ResearchView::setAssetProjectPath(const QString& _file)
{
    m_assetProjectFile = _file;
    setAssetIdentity(m_assetKind, m_assetId);
}

void ResearchView::setAssetIdentity(const QString& _kind, int _id)
{
    m_assetKind = _kind;
    m_assetId = _id;
    if (_kind == "characters" && m_characterAssets) {
        m_characterAssets->setContext(m_assetProjectFile, _kind, _id);
    } else if (_kind == "locations" && m_locationAssets) {
        m_locationAssets->setContext(m_assetProjectFile, _kind, _id);
    }
}

void ResearchView::editCharactersRoot()
""")

manager = root / "ManagementLayer/Research/ResearchManager.cpp"
edit(manager,
     "void ResearchManager::loadCurrentProjectSettings(const QString& _projectPath)\n{\n",
     """void ResearchManager::loadCurrentProjectSettings(const QString& _projectPath)
{
    m_view->setAssetProjectPath(_projectPath);
    if (m_currentResearch != nullptr) {
        const int type = m_currentResearch->type();
        if (type == Research::Character || type == Research::Location) {
            m_view->setAssetIdentity(type == Research::Character ? "characters" : "locations",
                                     m_currentResearch->id().value());
        }
    }
""")
edit(manager,
     "void ResearchManager::closeCurrentProject()\n{\n",
     """void ResearchManager::closeCurrentProject()
{
    m_view->setAssetProjectPath(QString());
""")
edit(manager,
     """                case Research::Character: {
                    auto* researchCharacter = dynamic_cast<Domain::ResearchCharacter*>(research);""",
     """                case Research::Character: {
                    m_view->setAssetIdentity("characters", research->id().value());
                    auto* researchCharacter = dynamic_cast<Domain::ResearchCharacter*>(research);""")
edit(manager,
     """                case Research::Location: {
                    m_view->editLocation(research->name(), research->description());""",
     """                case Research::Location: {
                    m_view->setAssetIdentity("locations", research->id().value());
                    m_view->editLocation(research->name(), research->description());""")
print("VN CZ 1.03 asset gallery source patches applied.")

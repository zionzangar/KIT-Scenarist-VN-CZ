#include "ExportDialog.h"
#include "ui_ExportDialog.h"

#include <BusinessLayer/Export/AbstractExporter.h>

#include <DataLayer/DataStorageLayer/StorageFacade.h>
#include <DataLayer/DataStorageLayer/SettingsStorage.h>

#include <3rd_party/Delegates/TreeViewItemDelegate/TreeViewItemDelegate.h>
#include <3rd_party/Helpers/FileHelper.h>
#include <3rd_party/Styles/TreeViewProxyStyle/TreeViewProxyStyle.h>

#include <QButtonGroup>
#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QToolBar>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>

using UserInterface::ExportDialog;

namespace {
    /**
     * @brief Ключ для доступа к папке с экспортируемыми документами
     */
    const QString EXPORT_FOLDER_KEY = "export/file-path";

    /**
     * @brief Индексы вкладок табов для экспорта
     */
    /** @{ */
    const int RESEARCH_TAB_INDEX = 0;
    const int OUTLINE_TAB_INDEX = 1;
    const int SCRIPT_TAB_INDEX = 2;
    /** @} */
}


ExportDialog::ExportDialog(QWidget* _parent) :
    QLightBoxDialog(_parent),
    m_ui(new Ui::ExportDialog)
{
    m_ui->setupUi(this);

    m_titlePageEditorButton = new QPushButton(tr("Title page editor"), this);
    m_ui->gridLayout_2->addWidget(m_titlePageEditorButton, 8, 0, 1, 4);
    connect(m_titlePageEditorButton, &QPushButton::clicked, this, &ExportDialog::editTitlePageTemplate);
    connect(m_ui->printTitle, &QCheckBox::toggled, m_titlePageEditorButton, &QPushButton::setEnabled);
    m_titlePageEditorButton->setEnabled(m_ui->printTitle->isChecked());

    initStyleSheet();
}

ExportDialog::~ExportDialog()
{
    delete m_ui;
}

void ExportDialog::setExportType(int _type)
{
    m_exportType = _type;
    if (m_ui->exportTypes->count() > _type) {
        m_ui->exportTypes->setCurrentIndex(_type);
    }
}

void ExportDialog::setResearchExportFilePath(const QString& _filePath)
{
    m_ui->researchFile->setText(_filePath);

    checkResearchExportAvailability();
}

void ExportDialog::setResearchExportFileName(const QString& _fileName)
{
    if (m_ui->researchFile->text().isEmpty()
        && m_researchExportFileName != _fileName) {
        m_researchExportFileName = _fileName;
        const QString exportFilePath =
                DataStorageLayer::StorageFacade::settingsStorage()->documentFilePath(EXPORT_FOLDER_KEY, _fileName + ".pdf");
        m_ui->researchFile->setText(exportFilePath);
        updateScriptFileFormat();
    }

    checkResearchExportAvailability();
}

void ExportDialog::setResearchModel(QAbstractItemModel* _model)
{
    m_ui->researchExportTree->setModel(_model);
}

void ExportDialog::setScriptExportFilePath(const QString& _filePath)
{
    if (!_filePath.isEmpty()) {
        m_ui->file->setText(_filePath);
        QFileInfo fileInfo(_filePath);
        if (fileInfo.suffix() == "docx") {
            m_ui->docx->setChecked(true);
        } else if (fileInfo.suffix() == "pdf") {
            m_ui->pdf->setChecked(true);
        } else if (fileInfo.suffix() == "fdx") {
            m_ui->fdx->setChecked(true);
        } else {
            m_ui->fountain->setChecked(true);
        }
    } else {
        m_ui->pdf->setChecked(true);
    }

    m_ui->file->setText(_filePath);
    checkScriptExportAvailability();
}

void ExportDialog::setScriptExportFileName(const QString& _fileName)
{
    if (m_ui->file->text().isEmpty()
        && m_scriptExportFileName != _fileName) {
        m_scriptExportFileName = _fileName;
        const QString exportFilePath =
                DataStorageLayer::StorageFacade::settingsStorage()->documentFilePath(EXPORT_FOLDER_KEY, _fileName);
        m_ui->file->setText(exportFilePath);
        updateScriptFileFormat();
    }

    checkScriptExportAvailability();
}

void ExportDialog::setCheckPageBreaks(bool _check)
{
    m_ui->checkPageBreaks->setChecked(_check);
}

void ExportDialog::setStylesModel(QAbstractItemModel* _model)
{
    m_ui->templates->setModel(_model);
}

void ExportDialog::setCurrentStyle(const QString& _styleName)
{
    //
    // Сбрасываем последний установленный шаблон
    //
    m_ui->templates->setCurrentIndex(-1);

    //
    // Если название шаблона задано, то установим его
    //
    if (!_styleName.isEmpty()) {
        m_ui->templates->setCurrentText(_styleName);
        emit currentStyleChanged(_styleName);
    }
    //
    // В противном случае выбираем первый из списка
    //
    else {
        m_ui->templates->setCurrentIndex(0);
    }
}

void ExportDialog::setPageNumbering(bool _isChecked)
{
    m_ui->pageNumbering->setChecked(_isChecked);
}

void ExportDialog::setScenesNumbering(bool _isChecked)
{
    m_ui->scenesNumbering->setChecked(_isChecked);
}

void ExportDialog::setDialoguesNumbering(bool _isChecked)
{
    m_ui->dialoguesNumbering->setChecked(_isChecked);
}

void ExportDialog::setSaveReviewMarks(bool _save)
{
    m_ui->saveReviewMarks->setChecked(_save);
}

void ExportDialog::setPrintTitle(bool _isChecked)
{
    m_ui->printTitle->setChecked(_isChecked);
    if (m_titlePageEditorButton != nullptr) {
        m_titlePageEditorButton->setEnabled(_isChecked);
    }
}

void ExportDialog::setTitlePageTemplate(const QString& _html)
{
    m_titlePageTemplateHtml = _html;
}

QString ExportDialog::titlePageTemplate() const
{
    return m_titlePageTemplateHtml;
}

void ExportDialog::setPrintWatermark(bool _isChecked)
{
    m_ui->printWatermark->setChecked(_isChecked);
}

void ExportDialog::setWatermark(const QString& _watermark)
{
    m_ui->watermark->setText(_watermark);
}

BusinessLogic::ExportParameters ExportDialog::exportParameters() const
{
    BusinessLogic::ExportParameters exportParameters;
    exportParameters.isResearch = m_exportType == RESEARCH_TAB_INDEX;
    exportParameters.isOutline = m_exportType == OUTLINE_TAB_INDEX;
    exportParameters.isScript = m_exportType == SCRIPT_TAB_INDEX;
    if (exportParameters.isResearch) {
        exportParameters.filePath = m_ui->researchFile->text();
    } else {
        exportParameters.filePath = m_ui->file->text();
    }
    exportParameters.checkPageBreaks = m_ui->checkPageBreaks->isChecked();
    exportParameters.style = m_ui->templates->currentText();
    exportParameters.printTilte = m_ui->printTitle->isChecked();
    exportParameters.titlePageTemplateHtml = m_titlePageTemplateHtml;
    exportParameters.printPagesNumbers = m_ui->pageNumbering->isChecked();
    exportParameters.printScenesNumbers = m_ui->scenesNumbering->isChecked();
    exportParameters.printDialoguesNumbers = m_ui->dialoguesNumbering->isChecked();
    exportParameters.saveReviewMarks = m_ui->saveReviewMarks->isChecked();
    exportParameters.printWatermark = m_ui->printWatermark->isChecked();
    exportParameters.watermark = m_ui->watermark->text();

    return exportParameters;
}

int ExportDialog::exportType() const
{
    return m_exportType;
}

QString ExportDialog::researchFilePath() const
{
    return m_ui->researchFile->text();
}

QString ExportDialog::scriptFilePath() const
{
    return m_ui->file->text();
}

QString ExportDialog::exportFormat() const
{
    QString format;
    if (m_exportType == RESEARCH_TAB_INDEX) {
        format = "pdf";
    } else {
        if (m_ui->docx->isChecked()) {
            format = "docx";
        } else if (m_ui->pdf->isChecked()) {
            format = "pdf";
        } else if (m_ui->fdx->isChecked()) {
            format = "fdx";
        } else {
            format = "fountain";
        }
    }
    return format;
}

void ExportDialog::setVisible(bool _visible)
{
    QLightBoxDialog::setVisible(_visible);

    m_ui->exportTypes->setCurrentIndex(m_exportType);
}

void ExportDialog::chooseResearchFile()
{
    const QString format = "pdf";
    const QString exportFolderPath =
            DataStorageLayer::StorageFacade::settingsStorage()->documentFolderPath(EXPORT_FOLDER_KEY);
    QString filePath =
            QFileDialog::getSaveFileName(this, tr("Choose file to export research"),
                (!m_ui->researchFile->text().isEmpty() ? m_ui->researchFile->text() : exportFolderPath),
                tr("%1 files (*%2)").arg(format.toUpper()).arg(format));

    if (!filePath.isEmpty()) {
        //
        // Обновить имя файла, если не задано расширение
        //
        if (!filePath.endsWith(format)) {
            filePath.append("." + format);
        }

        //
        // Сохраним путь к файлу
        //
        m_ui->researchFile->setText(filePath);
        DataStorageLayer::StorageFacade::settingsStorage()->saveDocumentFolderPath(EXPORT_FOLDER_KEY, filePath);
    }
}

void ExportDialog::updateScriptFileFormat()
{
    const QString format = exportFormat();
    QString filePath = m_ui->file->text();

    //
    // Обновить имя файла, если оно уже задано в другом формате
    //
    if (!filePath.isEmpty()
        && !filePath.endsWith(format)) {
        QFileInfo fileInfo(filePath);
        //
        // Если у файла есть расширение
        //
        if (!fileInfo.suffix().isEmpty()) {
            filePath.replace(fileInfo.suffix(), format);
        } else {
            filePath.append("." + format);
        }

        m_ui->file->setText(filePath);
    }

    updateParametersVisibility();
}

void ExportDialog::chooseScriptFile()
{
    QString format;
    if (m_ui->docx->isChecked()) {
        format = "docx";
    } else if (m_ui->pdf->isChecked()) {
        format = "pdf";
    } else if (m_ui->fdx->isChecked()) {
        format = "fdx";
    } else {
        format = "fountain";
    }
    const QString exportFolderPath =
            DataStorageLayer::StorageFacade::settingsStorage()->documentFolderPath(EXPORT_FOLDER_KEY);
    QString filePath =
            QFileDialog::getSaveFileName(this, tr("Choose file to export scenario"),
                (!m_ui->file->text().isEmpty() ? m_ui->file->text() : exportFolderPath),
                tr("%1 files (*%2)").arg(format.toUpper()).arg(format));

    if (!filePath.isEmpty()) {
        //
        // Сохраним путь к файлу
        //
        m_ui->file->setText(filePath);
        DataStorageLayer::StorageFacade::settingsStorage()->saveDocumentFolderPath(EXPORT_FOLDER_KEY, filePath);

        //
        // Обновим расширение файла
        //
        updateScriptFileFormat();
    }
}

void ExportDialog::checkResearchExportAvailability()
{
    checkExportAvailability(RESEARCH_TAB_INDEX);
}

void ExportDialog::checkScriptExportAvailability()
{
    checkExportAvailability(SCRIPT_TAB_INDEX);
}

void ExportDialog::checkExportAvailability(int _index)
{
    //
    // Проверяем в зависимости от того, какая вкладка активна в данный момент
    //
    QLineEdit* filePath = nullptr;
    QLabel* fileExists = nullptr;
    if (_index == RESEARCH_TAB_INDEX) {
        filePath = m_ui->researchFile;
        fileExists = m_ui->researchExistsLabel;
    } else {
        filePath = m_ui->file;
        fileExists = m_ui->existsLabel;
    }

    const int lastCursorPosition = filePath->cursorPosition();
    filePath->setText(FileHelper::systemSavebleFileName(filePath->text()));
    filePath->setCursorPosition(lastCursorPosition);

    const bool isFileExists = QFile::exists(filePath->text());
    fileExists->setVisible(isFileExists);

    m_ui->exportTo->setText(isFileExists ? tr("Rewrite") : tr("Export"));
    m_ui->exportTo->setEnabled(!filePath->text().isEmpty());
}

void ExportDialog::updateParametersVisibility()
{
    bool showTemplates = true;
    bool showPageNumbers = true;
    bool showDialoguesNumbers = true;
    bool showSaveReviewMarks = true;
    bool showCheckPageBreak = true;
    bool showPrintWatermark = m_ui->pdf->isChecked();
    //
    // Зависимость от вида
    //
    if (m_exportType == OUTLINE_TAB_INDEX) {
        showDialoguesNumbers = false;
    }
    //
    // Зависимость от типа
    //
    if (m_ui->fdx->isChecked()
        || m_ui->fountain->isChecked()) {
        showTemplates = false;
        showPageNumbers = false;
        showDialoguesNumbers = false;
        showSaveReviewMarks = false;
        showCheckPageBreak = false;
    }
    //
    // Скроем/покажем параметры
    //
    m_ui->templateLabel->setVisible(showTemplates);
    m_ui->templates->setVisible(showTemplates);
    m_ui->pageNumbering->setVisible(showPageNumbers);
    m_ui->dialoguesNumbering->setVisible(showDialoguesNumbers);
    m_ui->saveReviewMarks->setVisible(showSaveReviewMarks);
    m_ui->checkPageBreaks->setVisible(showCheckPageBreak);
    m_ui->printWatermark->setVisible(showPrintWatermark);
    m_ui->watermark->setVisible(showPrintWatermark);
}

void ExportDialog::editTitlePageTemplate()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Title page editor"));
    dialog.resize(760, 860);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QLabel* help = new QLabel(
        tr("Available variables: {TITLE}, {GENRE}, {AUTHOR}, {INFO}, {CONTACT}, {YEAR}. "
           "You can also add any custom text."), &dialog);
    help->setWordWrap(true);
    layout->addWidget(help);

    QToolBar* toolbar = new QToolBar(&dialog);
    QAction* bold = toolbar->addAction(QStringLiteral("B"));
    bold->setCheckable(true);
    QAction* italic = toolbar->addAction(QStringLiteral("I"));
    italic->setCheckable(true);
    QAction* underline = toolbar->addAction(QStringLiteral("U"));
    underline->setCheckable(true);
    toolbar->addSeparator();
    QAction* alignLeft = toolbar->addAction(tr("Left"));
    QAction* alignCenter = toolbar->addAction(tr("Center"));
    QAction* alignRight = toolbar->addAction(tr("Right"));
    toolbar->addSeparator();

    QComboBox* fontSize = new QComboBox(toolbar);
    const QStringList sizes = { "8", "9", "10", "11", "12", "14", "16", "18", "20", "24", "28", "32" };
    fontSize->addItems(sizes);
    fontSize->setCurrentText("12");
    toolbar->addWidget(fontSize);

    toolbar->addSeparator();
    QComboBox* placeholder = new QComboBox(toolbar);
    placeholder->addItems({ "{TITLE}", "{GENRE}", "{AUTHOR}", "{INFO}", "{CONTACT}", "{YEAR}" });
    toolbar->addWidget(placeholder);
    QPushButton* insertPlaceholder = new QPushButton(tr("Insert variable"), toolbar);
    toolbar->addWidget(insertPlaceholder);
    layout->addWidget(toolbar);

    QTextEdit* editor = new QTextEdit(&dialog);
    editor->setAcceptRichText(true);
    if (m_titlePageTemplateHtml.isEmpty()) {
        editor->setHtml(
            QStringLiteral(
                "<p align='center'><br><br><br><br><br><br><br><br><br><br>"
                "<span style='font-size:18pt; font-weight:600; text-decoration:underline;'>{TITLE}</span></p>"
                "<p align='center'><br><span style='font-size:11pt;'>{GENRE}</span></p>"
                "<p align='center'><br><br><span style='font-size:11pt;'>{AUTHOR}</span></p>"
                "<p align='left'><br><br><br><br><br><br><br><br>{INFO}</p>"
                "<p align='left'>{CONTACT}</p>"
                "<p align='center'><br>{YEAR}</p>"));
    } else {
        editor->setHtml(m_titlePageTemplateHtml);
    }
    layout->addWidget(editor, 1);

    connect(bold, &QAction::toggled, editor, [editor](bool checked) {
        QTextCharFormat f;
        f.setFontWeight(checked ? QFont::Bold : QFont::Normal);
        editor->mergeCurrentCharFormat(f);
    });
    connect(italic, &QAction::toggled, editor, [editor](bool checked) {
        QTextCharFormat f;
        f.setFontItalic(checked);
        editor->mergeCurrentCharFormat(f);
    });
    connect(underline, &QAction::toggled, editor, [editor](bool checked) {
        QTextCharFormat f;
        f.setFontUnderline(checked);
        editor->mergeCurrentCharFormat(f);
    });
    connect(alignLeft, &QAction::triggered, editor, [editor] { editor->setAlignment(Qt::AlignLeft); });
    connect(alignCenter, &QAction::triggered, editor, [editor] { editor->setAlignment(Qt::AlignCenter); });
    connect(alignRight, &QAction::triggered, editor, [editor] { editor->setAlignment(Qt::AlignRight); });
    connect(fontSize, &QComboBox::currentTextChanged, editor, [editor](const QString& size) {
        QTextCharFormat f;
        f.setFontPointSize(size.toDouble());
        editor->mergeCurrentCharFormat(f);
    });
    connect(insertPlaceholder, &QPushButton::clicked, editor, [editor, placeholder] {
        editor->textCursor().insertText(placeholder->currentText());
    });

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() == QDialog::Accepted) {
        m_titlePageTemplateHtml = editor->toHtml();
    }
}

void ExportDialog::initView()
{
    m_ui->exportParameters->setCurrentWidget(m_ui->researchExportPage);

    m_ui->exportTypes->addTab(tr("Export research"));
    m_ui->exportTypes->addTab(tr("Export outline"));
    m_ui->exportTypes->addTab(tr("Export script"));

    m_ui->researchExportTree->setItemDelegate(new TreeViewItemDelegate(m_ui->researchExportTree));
    m_ui->researchExportTree->setStyle(new TreeViewProxyStyle(m_ui->researchExportTree->style()));

    m_ui->browseFile->updateIcons();
    QButtonGroup* scriptFormatTypeGroup = new QButtonGroup(this);
    scriptFormatTypeGroup->addButton(m_ui->docx);
    scriptFormatTypeGroup->addButton(m_ui->pdf);
    scriptFormatTypeGroup->addButton(m_ui->fdx);
    scriptFormatTypeGroup->addButton(m_ui->fountain);
    m_ui->additionalSettings->setVisible(m_ui->showAdditional->isChecked());

    m_ui->researchBrowseFile->updateIcons();

    resize(width(), sizeHint().height());
}

void ExportDialog::initConnections()
{
    connect(m_ui->researchBrowseFile, &FlatButton::clicked, this, &ExportDialog::chooseResearchFile);
    connect(m_ui->researchFile, &QLineEdit::textChanged, this, &ExportDialog::checkResearchExportAvailability);

    //
    // Покажем страницу параметров экспорта в зависимости от выбранной вкладки
    //
    connect(m_ui->exportTypes, &TabBarExpanded::currentChanged, [this] (int _index) {
        m_exportType = _index;
        if (m_exportType == RESEARCH_TAB_INDEX) {
            m_ui->exportParameters->setCurrentWidget(m_ui->researchExportPage);
        } else {
            m_ui->exportParameters->setCurrentWidget(m_ui->scriptExportPage);
        }
        checkExportAvailability(m_exportType);
        updateParametersVisibility();
    });

    connect(m_ui->showAdditional, &QCheckBox::toggled, m_ui->additionalSettings, &QFrame::setVisible);

    connect(m_ui->templates, &QComboBox::currentTextChanged, this, &ExportDialog::currentStyleChanged);
    connect(m_ui->docx, &QRadioButton::toggled, this, &ExportDialog::updateScriptFileFormat);
    connect(m_ui->pdf, &QRadioButton::toggled, this, &ExportDialog::updateScriptFileFormat);
    connect(m_ui->fdx, &QRadioButton::toggled, this, &ExportDialog::updateScriptFileFormat);
    connect(m_ui->browseFile, &FlatButton::clicked, this, &ExportDialog::chooseScriptFile);
    connect(m_ui->file, &QLineEdit::textChanged, this, &ExportDialog::checkScriptExportAvailability);

    connect(m_ui->printWatermark, &QCheckBox::toggled, m_ui->watermark, &QLineEdit::setEnabled);

    connect(m_ui->cancel, &FlatButton::clicked, this, &ExportDialog::reject);
    connect(m_ui->printPreview, &FlatButton::clicked, this, &ExportDialog::printPreviewPressed);
    connect(m_ui->exportTo, &FlatButton::clicked, this, &ExportDialog::accept);
}

void ExportDialog::initStyleSheet()
{
    m_ui->toolbarLeftEmptyLabel->setProperty("inTopPanel", true);
    m_ui->toolbarLeftEmptyLabel->setProperty("topPanelTopBordered", true);
    m_ui->toolbarLeftEmptyLabel->setProperty("topPanelLeftBordered", true);
    m_ui->exportTypes->setProperty("inTopPanel", true);
    m_ui->toolbarRightEmptyLabel->setProperty("inTopPanel", true);
    m_ui->toolbarRightEmptyLabel->setProperty("topPanelTopBordered", true);
    m_ui->toolbarRightEmptyLabel->setProperty("topPanelRightBordered", true);
    m_ui->browseFile->setProperty("isBrowseButton", true);
    m_ui->researchBrowseFile->setProperty("isBrowseButton", true);
}

QWidget* ExportDialog::titleWidget() const
{
    return m_ui->toolbar;
}

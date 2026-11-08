// Waypoint Engine — editor main window implementation.
// SPDX-License-Identifier: Apache-2.0
#include "EditorWindow.hpp"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QKeySequence>
#include <QLineEdit>
#include <QList>
#include <QMenu>
#include <QStringList>
#include <QVariant>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QWidget>

#include <string>

#include "core/Log.hpp"

namespace {

QString toQString(const std::string& text) {
    return QString::fromStdString(text);
}

// Qt widgets live on the GUI thread, but the engine logs from everywhere. This
// bridges the two: invokeMethod with a queued connection is thread-safe.
void installLogBridge(EditorWindow* window) {
    waypoint::Log::setCallback([window](waypoint::LogLevel level, std::string_view message) {
        const char* levelName = "info";
        switch (level) {
            case waypoint::LogLevel::Warn: levelName = "warn"; break;
            case waypoint::LogLevel::Error: levelName = "error"; break;
            case waypoint::LogLevel::Critical: levelName = "critical"; break;
            case waypoint::LogLevel::Debug: levelName = "debug"; break;
            case waypoint::LogLevel::Trace: levelName = "trace"; break;
            default: break;
        }
        const QString text = QStringLiteral("[%1] %2")
                                 .arg(QString::fromLatin1(levelName),
                                      QString::fromStdString(std::string(message)));
        QMetaObject::invokeMethod(window, "appendLog", Qt::QueuedConnection, Q_ARG(QString, text));
    });
}

}  // namespace

EditorWindow::EditorWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Waypoint Editor");
    resize(1100, 700);

    createActions();
    createMenus();
    createDocks();

    m_statusLabel = new QLabel("No project open", this);
    statusBar()->addWidget(m_statusLabel);

    installLogBridge(this);

    appendLog("Waypoint Editor ready. File > New Project or File > Open Project.");
}

EditorWindow::~EditorWindow() {
    waypoint::Log::clearCallback();
}

void EditorWindow::createActions() {
    m_newProjectAction = new QAction("&New Project...", this);
    m_newProjectAction->setShortcut(QKeySequence::New);
    connect(m_newProjectAction, &QAction::triggered, this, &EditorWindow::onNewProject);

    m_openProjectAction = new QAction("&Open Project...", this);
    m_openProjectAction->setShortcut(QKeySequence::Open);
    connect(m_openProjectAction, &QAction::triggered, this, &EditorWindow::onOpenProject);

    m_saveSceneAction = new QAction("&Save Scene", this);
    m_saveSceneAction->setShortcut(QKeySequence::Save);
    connect(m_saveSceneAction, &QAction::triggered, this, &EditorWindow::onSaveScene);

    m_addEntityAction = new QAction("&Add Entity", this);
    m_addEntityAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(m_addEntityAction, &QAction::triggered, this, &EditorWindow::onAddEntity);

    m_deleteEntityAction = new QAction("&Delete Entity", this);
    m_deleteEntityAction->setShortcut(QKeySequence::Delete);
    connect(m_deleteEntityAction, &QAction::triggered, this, &EditorWindow::onDeleteEntity);

    m_quitAction = new QAction("&Quit", this);
    m_quitAction->setShortcut(QKeySequence::Quit);
    connect(m_quitAction, &QAction::triggered, this, &QWidget::close);
}

void EditorWindow::createMenus() {
    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(m_newProjectAction);
    fileMenu->addAction(m_openProjectAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveSceneAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_quitAction);

    QMenu* entityMenu = menuBar()->addMenu("&Entity");
    entityMenu->addAction(m_addEntityAction);
    entityMenu->addAction(m_deleteEntityAction);
}

void EditorWindow::createDocks() {
    m_sceneTree = new QTreeWidget(this);
    m_sceneTree->setHeaderLabels({"Entity", "Components"});
    m_sceneTree->setColumnWidth(0, 220);
    connect(m_sceneTree, &QTreeWidget::itemSelectionChanged, this,
            &EditorWindow::onSceneSelectionChanged);

    auto* sceneDock = new QDockWidget("Scene", this);
    sceneDock->setObjectName("sceneDock");
    sceneDock->setWidget(m_sceneTree);
    addDockWidget(Qt::LeftDockWidgetArea, sceneDock);

    m_console = new QPlainTextEdit(this);
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(5000);

    auto* consoleDock = new QDockWidget("Console", this);
    consoleDock->setObjectName("consoleDock");
    consoleDock->setWidget(m_console);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock);
}

void EditorWindow::openProject(const QString& directory) {
    if (!saveSceneIfDirty()) {
        return;
    }

    std::string error;
    auto project = waypoint::loadProject(directory.toStdString(), &error);
    if (!project.has_value()) {
        QMessageBox::warning(this, "Could not open project",
                             QString::fromStdString(error));
        appendLog(QString::fromStdString("Open project failed: " + error));
        return;
    }

    auto scene = waypoint::Scene::loadFromFile(project->startScenePath(), &error);
    if (!scene.has_value()) {
        QMessageBox::warning(this, "Could not open scene", QString::fromStdString(error));
        appendLog(QString::fromStdString("Open scene failed: " + error));
        return;
    }

    m_project = project;
    m_scene = std::move(scene);
    m_dirty = false;

    refreshTitle();
    refreshSceneTree();
    refreshStatus();
    appendLog(QString::fromStdString("Opened project '" + m_project->name + "'"));
}

void EditorWindow::onNewProject() {
    const QString parentDirectory = QFileDialog::getExistingDirectory(
        this, "Where should the project live?", QString());
    if (parentDirectory.isEmpty()) {
        return;
    }

    bool accepted = false;
    const QString name = QInputDialog::getText(this, "New project", "Project name:",
                                               QLineEdit::Normal, "MyGame", &accepted);
    if (!accepted || name.trimmed().isEmpty()) {
        return;
    }

    const std::filesystem::path directory =
        std::filesystem::path(parentDirectory.toStdString()) / name.trimmed().toStdString();

    std::string error;
    const auto project = waypoint::createProject(directory, name.trimmed().toStdString(), &error);
    if (!project.has_value()) {
        QMessageBox::warning(this, "Could not create project", QString::fromStdString(error));
        return;
    }

    openProject(QString::fromStdString(directory.generic_string()));
}

void EditorWindow::onOpenProject() {
    const QString directory =
        QFileDialog::getExistingDirectory(this, "Open a Waypoint project", QString());
    if (!directory.isEmpty()) {
        openProject(directory);
    }
}

void EditorWindow::onSaveScene() {
    if (!m_scene.has_value()) {
        QMessageBox::information(this, "Nothing to save", "Open or create a project first.");
        return;
    }
    std::filesystem::path path = m_scene->sourcePath();
    if (path.empty() && m_project.has_value()) {
        path = m_project->startScenePath();
    }
    if (path.empty()) {
        QMessageBox::information(this, "Nothing to save", "This scene has no file yet.");
        return;
    }

    std::string error;
    if (!m_scene->saveToFile(path, &error)) {
        QMessageBox::warning(this, "Could not save scene", QString::fromStdString(error));
        return;
    }
    m_scene->setSourcePath(path);
    m_dirty = false;
    refreshTitle();
    appendLog(QString::fromStdString("Saved " + path.generic_string()));
}

void EditorWindow::onAddEntity() {
    if (!m_scene.has_value()) {
        QMessageBox::information(this, "No scene", "Open or create a project first.");
        return;
    }
    const std::string name = "Entity " + std::to_string(m_scene->world().entityCount() + 1);
    m_scene->createEntity(name);
    m_dirty = true;
    refreshTitle();
    refreshSceneTree();
    refreshStatus();
    appendLog(QString::fromStdString("Added entity '" + name + "'"));
}

void EditorWindow::onDeleteEntity() {
    QTreeWidgetItem* item = selectedEntityItem();
    if (item == nullptr || !m_scene.has_value()) {
        return;
    }
    const QVariant data = item->data(0, Qt::UserRole);
    if (!data.isValid()) {
        return;  // the scene root
    }
    const auto handle = static_cast<waypoint::EntityHandle>(data.toULongLong());
    const QString name = item->text(0);
    if (m_scene->world().destroyEntity(handle)) {
        m_dirty = true;
        refreshTitle();
        refreshSceneTree();
        refreshStatus();
        appendLog(QString("Deleted entity '%1'").arg(name));
    }
}

void EditorWindow::onSceneSelectionChanged() {
    refreshStatus();
}

QTreeWidgetItem* EditorWindow::selectedEntityItem() const {
    const QList<QTreeWidgetItem*> selected = m_sceneTree->selectedItems();
    return selected.isEmpty() ? nullptr : selected.first();
}

void EditorWindow::refreshTitle() {
    QString title = "Waypoint Editor";
    if (m_project.has_value()) {
        title += QStringLiteral(" - ") + toQString(m_project->name);
    }
    if (m_scene.has_value()) {
        title += QStringLiteral(" [") + toQString(m_scene->name()) + "]";
    }
    if (m_dirty) {
        title += " *";
    }
    setWindowTitle(title);
}

void EditorWindow::refreshSceneTree() {
    m_sceneTree->clear();
    if (!m_scene.has_value()) {
        return;
    }

    auto* root = new QTreeWidgetItem(m_sceneTree);
    root->setText(0, toQString(m_scene->name()));
    root->setText(1, QString("%1 entities").arg(m_scene->world().entityCount()));
    root->setExpanded(true);

    for (const waypoint::EntityHandle handle : m_scene->world().entities()) {
        auto* item = new QTreeWidgetItem(root);
        const std::string* name = m_scene->world().name(handle);
        item->setText(0, name != nullptr ? toQString(*name) : QString("Entity"));
        item->setData(0, Qt::UserRole, static_cast<qulonglong>(handle));

        QStringList components;
        if (m_scene->world().has<waypoint::CameraComponent>(handle)) components << "camera";
        if (m_scene->world().has<waypoint::FlyCameraComponent>(handle)) components << "flyCamera";
        if (m_scene->world().has<waypoint::DirectionalLightComponent>(handle)) {
            components << "light";
        }
        if (m_scene->world().has<waypoint::MeshRendererComponent>(handle)) components << "mesh";
        if (m_scene->world().has<waypoint::RigidBodyComponent>(handle)) components << "rigidBody";
        if (m_scene->world().has<waypoint::ScriptComponent>(handle)) components << "script";
        item->setText(1, components.join(", "));
    }
    m_sceneTree->expandAll();
}

void EditorWindow::refreshStatus() {
    if (!m_scene.has_value()) {
        m_statusLabel->setText("No project open");
        return;
    }
    QString text = QString("%1 entities").arg(m_scene->world().entityCount());
    if (QTreeWidgetItem* item = selectedEntityItem(); item != nullptr) {
        const QVariant data = item->data(0, Qt::UserRole);
        if (data.isValid()) {
            text += QString(" | selected: %1").arg(item->text(0));
        }
    }
    if (m_project.has_value()) {
        text += QString(" | %1").arg(toQString(m_project->rootDirectory.generic_string()));
    }
    m_statusLabel->setText(text);
}

bool EditorWindow::saveSceneIfDirty() {
    if (!m_dirty) {
        return true;
    }
    const auto answer = QMessageBox::question(
        this, "Unsaved changes", "Save the current scene before continuing?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel) {
        return false;
    }
    if (answer == QMessageBox::Save) {
        onSaveScene();
        return !m_dirty;
    }
    return true;
}

void EditorWindow::appendLog(const QString& text) {
    m_console->appendPlainText(text);
}

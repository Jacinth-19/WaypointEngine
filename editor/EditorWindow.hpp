// Waypoint Engine — editor main window.
// SPDX-License-Identifier: Apache-2.0
//
// The first slice of the editor: open or create a project, see its entities,
// add and delete entities, save the scene, and read the engine log. A viewport
// and an inspector come next (see docs/ROADMAP.md).
#pragma once

#include <QMainWindow>
#include <QString>

#include <optional>

#include "project/Project.hpp"
#include "scene/Scene.hpp"

class QAction;
class QLabel;
class QPlainTextEdit;
class QTreeWidget;
class QTreeWidgetItem;

class EditorWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit EditorWindow(QWidget* parent = nullptr);
    ~EditorWindow() override;

    // Loads <directory>/waypoint.project.json and its start scene.
    void openProject(const QString& directory);

private slots:
    void onNewProject();
    void onOpenProject();
    void onSaveScene();
    void onAddEntity();
    void onDeleteEntity();
    void onSceneSelectionChanged();
    void appendLog(const QString& text);

private:
    void createActions();
    void createMenus();
    void createDocks();
    void refreshTitle();
    void refreshSceneTree();
    void refreshStatus();
    [[nodiscard]] bool saveSceneIfDirty();
    [[nodiscard]] QTreeWidgetItem* selectedEntityItem() const;

    std::optional<waypoint::Project> m_project;
    std::optional<waypoint::Scene> m_scene;
    bool m_dirty = false;

    QTreeWidget* m_sceneTree = nullptr;
    QPlainTextEdit* m_console = nullptr;
    QLabel* m_statusLabel = nullptr;

    QAction* m_newProjectAction = nullptr;
    QAction* m_openProjectAction = nullptr;
    QAction* m_saveSceneAction = nullptr;
    QAction* m_addEntityAction = nullptr;
    QAction* m_deleteEntityAction = nullptr;
    QAction* m_quitAction = nullptr;
};

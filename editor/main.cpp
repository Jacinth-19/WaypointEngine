// Waypoint Engine — editor entry point.
// SPDX-License-Identifier: Apache-2.0
#include <QApplication>
#include <QString>

#include "EditorWindow.hpp"
#include "core/Log.hpp"
#include "project/Project.hpp"

int main(int argc, char** argv) {
    waypoint::Log::init(waypoint::LogLevel::Info);

    QApplication application(argc, argv);
    QApplication::setApplicationName("Waypoint Editor");
    QApplication::setApplicationVersion(QString::fromStdString(std::string(waypoint::kEngineVersion)));
    QApplication::setOrganizationName("Waypoint Engine");

    EditorWindow window;
    window.show();

    // `waypoint_editor <project-folder>` opens straight into a project.
    if (argc > 1) {
        window.openProject(QString::fromLocal8Bit(argv[1]));
    }

    const int result = QApplication::exec();
    waypoint::Log::shutdown();
    return result;
}

/*
    Author: Marco Costalba (C) 2005-2007

    Copyright: See COPYING file that comes with this distribution

*/
#include "common.h"
#include "mainimpl.h"

#include "shared/defmac.h"
#include "shared/utils.h"
#include "shared/logger/logger.h"
#include "shared/logger/config.h"
#include "shared/config/appl_conf.h"
#include "shared/config/logger_conf.h"
#include "shared/qt/logger_operators.h"
#include "shared/qt/version_number.h"

#include <QSettings>

#define APPLICATION_NAME "QGit"

#if defined(_MSC_VER) && defined(NDEBUG)
    #pragma comment(linker,"/entry:mainCRTStartup")
    #pragma comment(linker,"/subsystem:windows")
#endif

using namespace QGit;

int main(int argc, char* argv[])
{
    alog::logger().start();

#ifdef NDEBUG
    alog::logger().addSaverStdOut(alog::Level::Info, true);
#else
    alog::logger().addSaverStdOut(alog::Level::Debug);
#endif

    log_info << log_format(
        "'%?' is running (version: %?; gitrev: %?)",
        APPLICATION_NAME, productVersion().toString(), GIT_REVISION);
    alog::logger().flush();

    QApplication app(argc, argv);
#if QT_VERSION >= QT_VERSION_CHECK(5,6,0)
    app.setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif
    QCoreApplication::setOrganizationName(ORG_KEY);
    QCoreApplication::setApplicationName(APP_KEY);

    /* On Windows msysgit exec directory is set up
     * during installation so to always find git.exe
     * also if not in PATH
     */
    QSettings set;
    GIT_DIR = set.value(GIT_DIR_KEY).toString();

    initMimePix();

    MainImpl* mainWin = new MainImpl;
    mainWin->show();
    chk_connect_a(&app, SIGNAL(lastWindowClosed()), &app, SLOT(quit()));
    bool ret = app.exec();

    freeMimePix();

    log_info << log_format("'%?' is stopped", APPLICATION_NAME);
    alog::logger().flush();
    alog::logger().waitingFlush();
    alog::logger().stop();

    return ret;
}

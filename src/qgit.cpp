/*
    Author: Marco Costalba (C) 2005-2007

    Copyright: See COPYING file that comes with this distribution

*/
#include "common.h"
#include "mainimpl.h"
#include "spellcheck/spellcheck.h"

#include "shared/defmac.h"
#include "shared/utils.h"
#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/logger/config.h"
#include "shared/config/appl_conf.h"
#include "shared/qt/logger_operators.h"
#include "shared/qt/version_number.h"

#define APPLICATION_NAME "QGit"

#if defined(_MSC_VER) && defined(NDEBUG)
    #pragma comment(linker,"/entry:mainCRTStartup")
    #pragma comment(linker,"/subsystem:windows")
#endif

using namespace qgit;

void stopLog()
{
    alog::logger().flush();
    alog::logger().waitingFlush();
    alog::logger().stop();
}

int main(int argc, char* argv[])
{
    alog::logger().start();

#ifdef NDEBUG
    alog::logger().addSaverStdOut(alog::Level::Info, true);
#else
    alog::logger().addSaverStdOut(alog::Level::Debug);
#endif

    QString configDir = CONFIG_DIR;
    config::dirExpansion(configDir);
    if (!QDir(configDir).exists())
        if (!QDir().mkpath(configDir))
        {
            log_error << "Failed create log directory: " << configDir;
            stopLog();
            return 1;
        }

    QString configFile  = configDir + "/qgit.conf3";
    config::base().readFile(configFile.toStdString());

    //bool configModify = false;

    //--- Create default config parameters ---
    QString extDiff = "kompare";
    if (!config::base().getValue("general.external_diff_viewer", extDiff))
         config::base().setValue("general.external_diff_viewer", extDiff);

    QString extEditor = "emacs";
    if (!config::base().getValue("general.external_editor", extEditor))
         config::base().setValue("general.external_editor", extEditor);

    int iconSizeIndex = 0;
    if (!config::base().getValue("general.icon_size_index", iconSizeIndex))
         config::base().setValue("general.icon_size_index", iconSizeIndex);

    std::string logLevelStr = "info";
    if (!config::base().getValue("logger.level", logLevelStr))
         config::base().setValue("logger.level", logLevelStr);

    bool logShortMessages = true;
    if (!config::base().getValue("logger.short_messages", logShortMessages))
         config::base().setValue("logger.short_messages", logShortMessages);

    QString exFile = ".git/info/exclude";
    if (!config::base().getValue("working_dir.exclude.from", exFile))
         config::base().setValue("working_dir.exclude.from", exFile);

    QString exPerDir = ".gitignore";
    if (!config::base().getValue("working_dir.exclude.per_directory", exPerDir))
         config::base().setValue("working_dir.exclude.per_directory", exPerDir);

    QString commitTmpl = ".git/commit-template";
    if (!config::base().getValue("commit.template_file_path", commitTmpl))
         config::base().setValue("commit.template_file_path", commitTmpl);

    if (config::base().changed())
        config::base().save();
    //---

    // Init logger
    alog::Level logLevel = alog::levelFromString(logLevelStr);
    alog::logger().addSaverStdOut(logLevel, logShortMessages);

    log_info << log_format(
        "%? is running (version: %?; gitrev: %?)",
        APPLICATION_NAME, productVersion().toString(), GIT_REVISION);
    alog::logger().flush();

    qgit::flags().load();

    QApplication app(argc, argv);
	app.setAttribute(Qt::AA_UseHighDpiPixmaps, true);
    QCoreApplication::setOrganizationName("qgit");
    QCoreApplication::setApplicationName("qgit");

    /* On Windows msysgit exec directory is set up
     * during installation so to always find git.exe
     * also if not in PATH
     */
    //QSettings set;
    //GIT_DIR = set.value(GIT_DIR_KEY).toString();

    initMimePix();

    MainImpl* mainWin = new MainImpl;
    mainWin->show();
    chk_connect_a(&app, SIGNAL(lastWindowClosed()), &app, SLOT(quit()));
    bool ret = app.exec();

    freeMimePix();
    spellCheck().deinit();

    if (config::base().changed())
        config::base().save();

    log_info << log_format("%? is stopped", APPLICATION_NAME);
    stopLog();

    return ret;
}

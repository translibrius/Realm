#include <entry.h>

b8 create_application(application *application) {
    application_config config = {
        .title = "Realm"
    };
    application->config = config;
    return true;
}

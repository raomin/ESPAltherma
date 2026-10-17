#ifndef ESPALTHERMA_VERSION_H
#define ESPALTHERMA_VERSION_H

// Overridden by CI release builds (-DESPALTHERMA_VERSION=\"x.y.z\")
#ifndef ESPALTHERMA_VERSION
#define ESPALTHERMA_VERSION "2.0.0-dev"
#endif

// Overridden by CI builds: the time of the commit (Unix seconds). The web update compares builds with it: a
// development build has no version order.
#ifndef ESPALTHERMA_BUILD_TIME
#define ESPALTHERMA_BUILD_TIME 0
#endif

#endif

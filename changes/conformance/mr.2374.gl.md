Fix: Not all Android devices have `/sdcard` as a writable path, use the `app->activity->externalDataPath` path unless the build system overrides this.

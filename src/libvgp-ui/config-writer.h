#ifndef VGP_CONFIG_WRITER_H
#define VGP_CONFIG_WRITER_H

/* Simple TOML config writer that preserves structure.
 * Reads existing file, updates specified key=value pairs,
 * writes back. Preserves comments and ordering. */

/* Set a value in a TOML file. Creates section and key if they don't exist.
 * path: file path
 * section: TOML section name (e.g., "general", "panel.widgets.left")
 * key: key name
 * value: new value (will be quoted if it contains spaces)
 * Returns 0 on success, -1 on error. */
int config_set_value(const char *path, const char *section,
                      const char *key, const char *value);

/* Set a numeric value */
int config_set_int(const char *path, const char *section,
                    const char *key, int value);

/* Set a float value */
int config_set_float(const char *path, const char *section,
                      const char *key, float value);

#endif /* VGP_CONFIG_WRITER_H */

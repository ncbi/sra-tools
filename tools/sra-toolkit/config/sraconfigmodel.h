#ifndef SRACONFIGCONTROLLER_H
#define SRACONFIGCONTROLLER_H

#include <stdint.h>

#include <QObject>

class vdbconf_model;
struct KNgcObj;


enum RootState
{
    RootState_OK,            // successfully changed repository root
    RootState_NotChanged,    // the new path is the same as the old one
    RootState_NotUnique,   // there is another repository with the same root
    RootState_MkdirFail,     // failed to make new repository directory
    RootState_NewPathEmpty,  // new repository directory path is not empty
    RootState_NewDirNotEmpty,// new repository directory is not empty
    RootState_NewNotDir,     // new repository exists and is not a directory
    RootState_OldNotEmpty,   // old repository is not empty
    RootState_Error,         // some unusual error happened
};

class SRAConfigModel : public QObject
{
    Q_OBJECT

public:

    bool commit ();
    bool config_changed ();
    bool path_exists ( std :: string &path );

    void create_directory ( const KNgcObj *ngc );
    void reload ();


    // cache
    bool global_cache_enabled ();
    void set_global_cache_enabled ( bool enabled );

    bool user_cache_enabled ();
    void set_user_cache_enabled ( bool enabled );

    // network
    bool remote_enabled ();
    void set_remote_enabled ( bool enabled );

    bool site_enabled ();
    void set_site_enabled ( bool enabled );

    // ngc
    bool import_ngc ( const std :: string path, const KNgcObj *ngc, uint32_t permissions, uint32_t *rslt_flags );
    bool get_ngc_obj_id ( const KNgcObj *ngc, uint32_t *id );
    std :: string get_ngc_root ( std :: string &dir, const KNgcObj *ngc );

    // proxy
    bool proxy_enabled ();
    void set_proxy_enabled ( bool enabled );

    std :: string get_proxy_path ();
    void set_proxy_path ( const std :: string &path );

    bool proxy_priority ();
    void set_proxy_priority ( bool priority );

    // settings
    std :: string get_current_path ();
    std :: string get_home_path ();

    std :: string get_public_path ();
    RootState set_public_path ( std :: string &path, bool flushOld, bool reuseNew );

    std :: string get_user_default_path ();
    void set_user_default_path ( const char *path );

    // workspace
    bool site_workspace_exists ();

    RootState configure_workspace ( const std :: string &path, bool reuseNew = false );

    uint32_t workspace_count ();
    int32_t get_workspace_id ( const std :: string &name );

    std :: string get_workspace_name ( uint32_t id );
    std :: string get_workspace_path ( uint32_t id );

    RootState set_workspace_path ( const std :: string &path, uint32_t id, bool flushOld, bool reuseNew );

    // constructor
    SRAConfigModel ( vdbconf_model &config_model, QObject *parent = 0 );

signals:

    void dirty_config ();

private:

    vdbconf_model &model;

};

#endif // SRACONFIGCONTROLLER_H

#ifndef SRACONFIGCONTROLLER_H
#define SRACONFIGCONTROLLER_H

#include <QObject>

class vdbconf_model;
struct KNgcObj;

class SRAConfigModel : public QObject
{
    Q_OBJECT

public:

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

    bool config_changed ();
    bool proxy_enabled ();
    bool proxy_priority ();
    bool remote_enabled ();
    bool site_enabled ();
    bool import_ngc ( const std :: string path,  KNgcObj *ngc, uint32_t permissions, uint32_t *rslt_flags );
    bool path_exists ( const std :: string &path );
    bool site_workspace_exists ( const std :: string &path );

    void commit ();
    void reload ();
    void create_directory ( const KNgcObj *ngc );

    void set_cache_enabled ( bool enabled );
    void set_proxy_enabled ( bool enabled );
    void set_remote_enabled ( bool enabled );
    void set_site_enabled ( bool enabled );
    void set_proxy_priority ( bool priority );
    void set_public_path ( const std :: string &path, bool flushOld, bool reuseNew );
    void set_workspace_path ( const std :: string &path, uint32_t id, bool flushOld, bool reuseNew );

    std :: string get_current_path ();
    std :: string get_home_path ();
    std :: string get_proxy_path ();
    std :: string get_public_path ();
    std :: string get_user_default_path ();

    bool get_ngc_obj_id ( const KNgcObj *ngc, uint32_t *id );
    int32_t get_workspace_id ( const std :: string &name );
    std :: string get_ngc_root ( std :: string &dir, KNgcObj *ngc );
    std :: string get_workspace_name ( uint32_t id );
    std :: string get_workspace_path ( uint32_t id );

    RootState configure_workspace ( const std :: string &path, bool reuseNew = false );

    SRAConfigModel ( vdbconf_model &config_model, QObject *parent = 0 );

signals:

    void dirty_config ();

private:

    vdbconf_model &model;

};

#endif // SRACONFIGCONTROLLER_H

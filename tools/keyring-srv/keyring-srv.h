/*===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 */

#ifndef _h_keyring_srv_
#define _h_keyring_srv_

#include <klib/rc.h>
#include <klib/refcount.h>

struct KFile;
struct String;

typedef struct KKeyRing KKeyRing;
struct KKeyRing;

#define KR_PWD_PROMPT_1 "Password:\n"
#define KR_PWD_PROMPT_2 "Retype Password:\n"

/* KeyRingOpen 
 * kr - [ OUT ] the keyring object
 * path - [ IN ] the POSIX path to the database file
 * pwd_in - [ IN ] a file object to read database password from
 * pwd_out - [ IN ] a file object to print password prompts to
 *
 * If the database file exists, will prompt to pwd_out for a password, read the password from pwd_in, and use the password to open the database
 * If the database file does not exist, will prompt for a password twice and create the database if passwords match
 *
 * returns:
 *  - database file exists, the supplied password is incorrect
 *  - database file does not exist, passwords do not match
 */
extern rc_t KeyRingOpen(KKeyRing** kr, const char* path, const struct KFile* pwd_in, struct KFile* pwd_out); 

/* KeyRingAddRef
 */
extern rc_t KeyRingAddRef(KKeyRing* kr);

/* KeyRingRelease
 */
extern rc_t KeyRingRelease(KKeyRing* kr);

/* KeyRingAddProject
 * Saves the project with associated download ticket and encryption key. 
 * If the project under this name already exists, the ticket/key will be overwritten as necessary.
 * Will update the database file. May block/timeout if the file is locked.
 */
extern rc_t KeyRingAddProject(KKeyRing* kr, 
                              const struct String* name, 
                              const struct String* download_ticket, 
                              const struct String* encryption_key);

/* KeyRingGetProject
 *  Retrieves a project's download ticket and encryption key by the project's name.
 *  (TODO: Keys are returned as pointers into shared memory)
 *  (TODO: Should it check if the database file needs to be reloaded?)
 */
extern rc_t KeyRingGetProject(KKeyRing* kr, const struct String* name, struct String* download_ticket, struct String* encryption_key);

/* KeyRingAddObject
 *  Saves an object in association with a project.
 *  If the project under this name already exists, it will be overwritten as necessary.
 *  Will update the database file. May block/timeout if the file is locked.
 */
extern rc_t KeyRingAddObject(KKeyRing* kr, 
                             const struct String* object_name, 
                             const struct String* project_name, 
                             const struct String* display_name, 
                             uint64_t size, 
                             const struct String* checksum);

/* KeyRingGetKey
 *  Retrieves an encryption key associated with the given object.
 *  (TODO: Keys are returned as pointers into shared memory)
 *  (TODO: Should it check if the database file needs to be reloaded?)
 */
extern rc_t KeyRingGetKey(KKeyRing* kr, const struct String* object_name, struct String* encryption_key);
                             
#endif /* _h_keyring_srv_ */

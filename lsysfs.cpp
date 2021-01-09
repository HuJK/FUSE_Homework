/**
 * Less Simple, Yet Stupid Filesystem.
 * 
 * Mohammed Q. Hussain - http://www.maastaar.net
 *
 * This is an example of using FUSE to build a simple filesystem. It is a part of a tutorial in MQH Blog with the title "Writing Less Simple, Yet Stupid Filesystem Using FUSE in C": http://maastaar.net/fuse/linux/filesystem/c/2019/09/28/writing-less-simple-yet-stupid-filesystem-using-FUSE-in-C/
 *
 * License: GNU GPL
 */
 
#define FUSE_USE_VERSION 30


#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

// ... //

#include <unordered_map>
#include <string>
#include <any>
#include <iostream>
#include <vector>
using namespace std;

ostream nullstream(0);
ostream& fakecout = nullstream;
//#define cout fakecout

char dir_list[ 256 ][ 256 ];
int curr_dir_idx = -1;

char files_list[ 256 ][ 256 ];
int curr_file_idx = -1;

char files_content[ 256 ][ 256 ];
int curr_file_content_idx = -1;



class file{
public:
	struct stat attr;
	string body;
	bool valid = true;
};
class folder{
public:
	struct stat attr;
	unordered_map<std::string, folder*> folders;
	unordered_map<std::string, file*> files;
	bool valid = true;

	~folder(){
		for( pair<string,folder*> f: folders){
			delete f.second;
		}
		for( pair<string,file*> f: files){
			delete f.second;
		}
	}
};

folder* root;
folder* invalid;
file* invalid_file;
struct stat defaultattr;
struct stat defaultfileattr;

template <class T> 
bool map_has_key(T* a, string b)
{
	if(b == "."){
		return true;
	}
    return a->find(b) != a->end();
}

pair<folder*, string> get_parent(const char *path){
	folder* current = root;
	folder* parent = root;
	string objname = ".";
	int prev_loc = 0;
	for(int c = 0; path[c==0?0:c-1] != '\0'; c++){
		if(path[c] == '/' || path[c] == '\0'){
			if( c == prev_loc){
				continue;
			}
			//get folder name
			const int name_len = c-prev_loc-1;
			char name[name_len+1];
			strncpy(name, &path[prev_loc+1], name_len);
			name[name_len] = '\0';

			if(parent != current){
				parent = current;
			}
			if( ! map_has_key( &current->folders,name) ){
				current = invalid;
			}
			else{
				current = current->folders[name];
			}
			objname = name;
			prev_loc = c;
		}
	}

	return make_pair(parent,objname==""?".":objname);
}

folder* get_dir(const char *path){
	auto info = get_parent(path);
	folder* parent = info.first;
	string name = info.second;
	if(name == "."){
		return parent;
	}
	else if(map_has_key( &parent->folders ,name)){
		return parent->folders[name];
	}
	else{
		return invalid;
	}
}

folder* get_subfolder(folder* parent,string name){
	if(name == "."){
		return parent;
	}
	else if(map_has_key( &parent->folders,name)){
		return parent->folders[name];
	}
	return invalid;
}
// ... //

static int do_getattr( const char *path, struct stat *st )
{
	auto info = get_parent(path);
	auto parent = info.first;
	auto name = info.second;
	cout << "GET\t" << path;
	if(map_has_key(&parent->folders,name)){
		memcpy(st,&(get_subfolder(parent,name)->attr),sizeof(struct stat));
	}
	else if(map_has_key(&parent->files,name)){
		memcpy(st,&parent->files[name]->attr,sizeof(struct stat));
	}
	else{
		cout << "\tErr\n";
		return -ENOENT;
	}
	cout << "\tOK\n";
	return 0;
}

static int do_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi )
{
	filler( buffer, ".", NULL, 0 ); // Current Directory
	filler( buffer, "..", NULL, 0 ); // Parent Directory
	cout << "READ\t" << path;
	folder* current = get_dir(path);
	if(current->valid == false){
		cout << "\tErr\n";
		return -ENOENT;
	}
	for(auto f :current->folders){
		filler(buffer,f.first.c_str(),&f.second->attr,0);
	}

	for(auto f :current->files){
		filler(buffer,f.first.c_str(),&f.second->attr,0);
	}
	cout << "\tOK\n";
	return 0;
}

static int do_read( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi )
{
	cout << "READF\t" << path;
	auto info = get_parent(path);
	auto parent = info.first;
	auto name = info.second;

	if(! map_has_key( &parent->files,name)){
		cout << "\tErr\n";
		return -ENOENT;
	}
	
	memcpy( buffer, &parent->files[name]->body[offset], size );
	cout << "\tOK\n";
	return min(parent->files[name]->body.length(),offset+size) - offset;
}

static int do_mkdir( const char *path, mode_t mode )
{
	cout << "MKDIR\t" << path;
	auto info = get_parent(path);
	auto parent = info.first;
	auto name = info.second;
	if(map_has_key(&parent->folders,name)){
		cout << "\tErr\n";
		return -EEXIST;
	}
	else if(map_has_key(&parent->files,name)){
		cout << "\tErr\n";
		return -EEXIST;
	}
	if(parent->valid == false){
		cout << "\tErr\n";
		return -ENOENT;
	}
	
	folder* newf = new folder();
	memcpy( &newf->attr , &defaultattr, sizeof(struct stat));
	parent->folders.insert( make_pair( name,newf));
	cout << "\tOK\n";
	return 0;
}

static int do_mknod( const char *path, mode_t mode, dev_t rdev )
{
	cout << "MKNOD\t" << path;
	auto info = get_parent(path);
	auto parent = info.first;
	auto name = info.second;
	if(map_has_key(&parent->folders,name)){
		cout << "\tErr\n";
		return -EEXIST;
	}
	else if(map_has_key(&parent->files,name)){
		cout << "\tErr\n";
		return -EEXIST;
	}
	if(parent->valid == false){
		cout << "\tErr\n";
		return -ENOENT;
	}

	file* newf = new file();
	memcpy( &newf->attr , &defaultfileattr, sizeof(struct stat));
	newf->attr.st_nlink = 1;
	parent->files.insert(make_pair( name,newf));
	cout << "\tOK\n";
	return 0;
}

static int do_write( const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *info )
{
	cout << "WRITE\t" << path;
	auto parentinfo = get_parent(path);
	auto parent = parentinfo.first;
	auto name = parentinfo.second;
	if(! map_has_key(&parent->files,name)){
		cout << "\tErr\n";
		return -ENOENT;
	}
	file* f = parent->files[name];
	size_t body_len = f->body.length();
	if(size + offset > body_len){
		f->body.resize(size + offset);
	}
	memcpy( &f->body[offset],buffer,size);

	f->attr.st_size = f->body.length();
	f->attr.st_blocks = f->body.length() /512 +1;
	cout << "\tOK\n";
	return size;
}

static int do_truncate( const char path[], off_t new_length)
{
	cout << "TRUN\t" << path;
	auto parentinfo = get_parent(path);
	auto parent = parentinfo.first;
	auto name = parentinfo.second;
	if(! map_has_key(&parent->files,name)){
		cout << "\tErr\n";
		return -ENOENT;
	}
	file* f = parent->files[name];

	f->body.resize(new_length,'\0');

	f->attr.st_size = f->body.length();
	f->attr.st_blocks = f->body.length() / 512 +1;
	cout << "\tOK\n";
	return 0;
}

static int do_unlink (const char* path){
	cout << "DEL\t" << path;
	auto parentinfo = get_parent(path);
	auto parent = parentinfo.first;
	auto name = parentinfo.second;
	if(! map_has_key(&parent->files,name)){
		cout << "\tErr\n";
		return -ENOENT;
	}
	delete parent->files[name];
	parent->files.erase(name);
	cout << "\tOK\n";
	return 0;
}
static int do_rmdir (const char* path){
	cout << "RMDIR\t" << path;
	auto parentinfo = get_parent(path);
	auto parent = parentinfo.first;
	auto name = parentinfo.second;
	if(! map_has_key(&parent->folders,name)){
		cout << "\tErr\n";
		return -ENOENT;
	}
	delete parent->folders[name];
	parent->folders.erase(name);
	cout << "\tOK\n";
	return 0;
}

static int do_rename (const char* path_old, const char* path_new){
	cout << "MV\t" << path_old << "\t" << path_new;
	auto parentinfo = get_parent(path_old); 
	auto parent = parentinfo.first;
	auto name = parentinfo.second;


	//check dist is ok
	auto parentinfo_new = get_parent(path_new); 
	auto parent_new = parentinfo_new.first;
	auto name_new = parentinfo_new.second;
	if( map_has_key(&parent_new->folders,name_new)){
		do_rmdir(path_new);
	}
	else if(map_has_key(&parent_new->files,name_new)){
		do_unlink(path_new);
	}

	//move 
	if( map_has_key(&parent->folders,name)){
		parent_new->folders.insert(make_pair(name_new,parent->folders[name]));
		parent->folders.erase(name);
		cout << "\tOK\n";
		return 0;
	}
	else if(map_has_key(&parent->files,name)){
		parent_new->files.insert(make_pair(name_new,parent->files[name]));
		parent->files.erase(name);
		cout << "\tOK\n";
		return 0;
	}
	cout << "\tErr\n";
	return -ENOENT;
}

static int do_chmod(const char path[],mode_t mode){
	auto info = get_parent(path);
	auto parent = info.first;
	auto name = info.second;
	cout << "CHMOD\t" << path;
	if(map_has_key(&parent->folders,name)){
		get_subfolder(parent,name)->attr.st_mode = mode | S_IFDIR;
	}
	else if(map_has_key(&parent->files,name)){
		parent->files[name]->attr.st_mode = mode | S_IFREG;
	}
	else{
		cout << "\tErr\n";
		return -ENOENT;
	}
	cout << "\tOK\n";
	return 0;
}

static int do_chown(const char path[],uid_t uid,gid_t gid){
	auto info = get_parent(path);
	auto parent = info.first;
	auto name = info.second;
	cout << "CHOWN\t" << path;
	if(map_has_key(&parent->folders,name)){
		get_subfolder(parent,name)->attr.st_uid = uid;
		get_subfolder(parent,name)->attr.st_gid = gid;
	}
	else if(map_has_key(&parent->files,name)){
		parent->files[name]->attr.st_uid = uid;
		parent->files[name]->attr.st_gid = gid;
	}
	else{
		cout << "\tErr\n";
		return -ENOENT;
	}
	cout << "\tOK\n";
	return 0;
}

static struct fuse_operations operations = {
    .getattr	= do_getattr,
	.mknod		= do_mknod,
	.mkdir		= do_mkdir,
	.unlink     = do_unlink,
	.rmdir      = do_rmdir,
	.rename     = do_rename,
	.chmod      = do_chmod,
	.chown      = do_chown,
	.truncate   = do_truncate,
	.read		= do_read,
	.write      = do_write,
    .readdir    = do_readdir,
};

int main( int argc, char *argv[] )
{
	root = new folder();
	invalid = new folder();
	invalid_file = new file();


	defaultattr.st_uid = getuid();;
	defaultattr.st_gid = getgid();
	defaultattr.st_mode = 0777 | S_IFDIR;
	defaultattr.st_nlink = 2;
	defaultattr.st_atime = time( NULL ); // The last "a"ccess of the file/directory is right now
	defaultattr.st_mtime = time( NULL ); // The last "m"odification of the file/directory is right now
	defaultattr.st_size = 4096;
	defaultattr.st_blksize = 1;
	memcpy(&defaultfileattr,&defaultattr,sizeof(struct stat));
	defaultfileattr.st_nlink = 1;
	defaultfileattr.st_mode = 0777 | S_IFREG ;

	memcpy(&root->attr,&defaultattr,sizeof(struct stat));
	memcpy(&invalid->attr,&defaultattr,sizeof(struct stat));
	

	invalid->valid = false;
	invalid_file->valid = false;
	//void* testbuf = malloc(10000);
	//do_getattr("/", (struct stat*)testbuf );

	return fuse_main( argc, argv, &operations, NULL );
}

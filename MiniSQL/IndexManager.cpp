#include "IndexManager.h"

Buffer::BufferManager Index::Tree::BM = Buffer::BufferManager();

int Index::Node::findKeyLoc(std::string _key){
	// possible optimization: binary search
	int loc = 0;
	while(loc < key.size() && _key >= key[loc]){
		loc++;
	}
	return loc;
}

Index::Node::operator std::string(){
	// id#\n
	// parent#\n
	// isLeaf#\n
	// key[0]#key[1]#...#key[n-1]#\n
	// ptr[0]#...#ptr[n-1]#\n
	// ####...#\n
	std::string ret = std::to_string(id) + "#\n" 
		+ std::to_string(parent) + "#\n"
		+ std::to_string(isLeaf) + "#\n";
	for(auto i: key){
		ret += i;
		ret += "#";
	}
	ret += "\n";
	for(auto i: ptr){
		ret += std::to_string(i);
		ret += "#";
	}
	ret += "\n";
	for(int i = ret.length(); i < Buffer::BLOCKCAPACITY - 1; i++)
		ret += "#";
	ret += "\n";
	return ret;
}

Index::Node::Node(std::string _s){
	std::string current = "";
	int status = 0;
	for(auto c: _s){
		if(c == '\n'){
			status++;
			current = "";
		}
		else if(c == '#'){
			if(status == 0) id = std::stoi(current);
			else if (status == 1) parent = std::stoi(current);
			else if (status == 2) isLeaf = std::stoi(current);
			else if (status == 3) key.push_back(current);
			else if (status == 4) ptr.push_back(std::stoi(current));
			else /* reading place-holder '#', do nothing */;
			current = "";
		}
		else current += c;
	}
}

Index::Node::Node(int _id){
	id = _id;
	parent = -1;
	isLeaf = true;
}

Index::Node Index::Tree::readNodeFromDisk(int loc){
	BM.NewPage();
	BM.SetFilename("../test/" + name + ".index");
	BM.SetFileOffset(loc * Buffer::BLOCKCAPACITY);
	BM.Load();
	return Node(BM.Read());
}

void Index::Tree::writeNodeToDisk(Node _n){
	BM.NewPage();
	BM.Write(_n);
	BM.SetFilename("../test/" + name + ".index");
	BM.SetFileOffset(_n.id * Buffer::BLOCKCAPACITY);
	BM.Save();
}

void Index::Tree::splitNode(Node _n){
	if(_n.key.size() < degree){
		// no need to split
		writeNodeToDisk(_n);
		return;
	}
	Node sibling(size++);
	sibling.isLeaf = _n.isLeaf;
	sibling.parent = _n.parent;
	int mid = _n.key.size() / 2;
	for(int i = mid; i < _n.key.size(); i++){
		sibling.key.push_back(_n.key[i]);
		sibling.ptr.push_back(_n.ptr[i]);
	}
	_n.key.resize(mid);
	_n.ptr.resize(mid);

	// update the parent's information
	Node parent;
	if(_n.parent == -1){
		// a new root
		parent = Node(size++);
		parent.isLeaf = false;
		parent.ptr.push_back(_n.id);
		_n.parent = parent.id;
		sibling.parent = parent.id;
		root = parent.id;
	}
	else{
		// load the parent from disk
		parent = readNodeFromDisk(_n.parent);
	}
	int loc = parent.findKeyLoc(_n.key[0]);
	parent.key.insert(parent.key.begin() + loc, sibling.key[0]);
	parent.ptr.insert(parent.ptr.begin() + loc + 1, sibling.id);

	// save changes to disk
	writeNodeToDisk(_n);
	writeNodeToDisk(sibling);

	// recursively go up - check the parent
	splitNode(parent);
}

int Index::Tree::find(std::string _key){
	Node node = readNodeFromDisk(root);
	while(!node.isLeaf){// until this is a leaf node, do:
		int childNo = node.findKeyLoc(_key);
		node = readNodeFromDisk(node.ptr[childNo]);
	}
	int cnt = 0;
	for(auto s: node.key){
		if(s == _key) return node.ptr[cnt];
		cnt++;
	}
	throw("Key " + _key + " doesn't exist");
}

void Index::Tree::insert(std::string _key, int _value){
	Node node = readNodeFromDisk(root);// read the root from disk
	while(!node.isLeaf){// until this is a leaf node, do:
		int childNo = node.findKeyLoc(_key);
		node = readNodeFromDisk(node.ptr[childNo]);
	}
	int loc = node.findKeyLoc(_key);
	node.key.insert(node.key.begin() + loc, _key);
	node.ptr.insert(node.ptr.begin() + loc, _value);
	splitNode(node);
}

Index::Tree::Tree(std::string _name, int _datawidth){
	name = _name;
	degree = (Buffer::BLOCKCAPACITY - 2*10) / (_datawidth + 10) - 1;
	// 10: bytes per Node id
	BM.NewPage();
	BM.SetFilename("../test/" + name + ".index");
	if(BM.IsExist()){
		// load this index from disk
		while(BM.Load()){
			size++;
			Node node = Node(BM.Read());
			if(node.parent == -1) root = node.id;// find the root
			BM.SetSize(0);
		}
	}
	else{
		// create a new file for this index on disk
		size = 1;
		root = 0;
		BM.Write(Node(0));// root of the B+ tree
		BM.Save();
	}
}

Index::Tree::Tree(){
	// this tree doesn't exist
}

void Index::Tree::destroy(){
	BM.NewPage();
	BM.SetFilename("../test/" + name + ".index");
	BM.Delete();
}

void Index::IndexManager::setWorkspace(std::string _table, std::string _attribute){
	workspace = _table + "#" + _attribute;
}

void Index::IndexManager::createIndex(int _datasize){
	if(trees.find(workspace) != trees.end())
		throw("Index on " + workspace + " already existed.");
	trees[workspace] = Tree(workspace, _datasize);
}

void Index::IndexManager::dropIndex(){
	trees[workspace].destroy();
	trees.erase(workspace);
}

int Index::IndexManager::find(std::string _key){
	return trees[workspace].find(_key);
}

void Index::IndexManager::insert(std::string _key, int _value){
	trees[workspace].insert(_key, _value);
}

void Index::IndexManager::sample(){
	Index::IndexManager IM;
	IM.setWorkspace("student", "sname");
	IM.createIndex(100);
	while(1)
		try{
			std::string cmd, key;
			std::cin >> cmd >> key;
			if(cmd == "ins"){
				int value;
				std::cin >> value;
				IM.insert(key, value);
			}
			else{
				std::cout << IM.find(key) << std::endl;
			}
		}
		catch(std::string errmsg){
			std::cout << errmsg << std::endl;
		}
}
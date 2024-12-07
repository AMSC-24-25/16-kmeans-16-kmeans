#include "KDTree.hpp"

template <typename PT, std::size_t PD>
KDNode<PT, PD>* KDTree<PT, PD>::buildTree(typename std::vector<Point<PT, PD>>::iterator start,
                                           typename std::vector<Point<PT, PD>>::iterator end, 
                                           int level_index) {
    if (start >= end) 
        return nullptr;

    int axis = level_index % PD;

    auto points_lenght = end - start;
    auto middle = start + points_lenght / 2;

    auto compare_lambda = [axis](const Point<PT, PD> &p1,
                                 const Point<PT, PD> &p2) {
        return p1[axis] < p2[axis];
    };

    std::nth_element(start, middle, end, compare_lambda); 
    
    //...//

    KDNode<PT, PD>* new_node = new KDNode<PT, PD>(*middle);
    new_node->left_node_ = buildTree(start, middle, level_index + 1);
    new_node->right_node_ = buildTree(middle + 1, end, level_index + 1);
    return new_node;        
}

template <typename PT, std::size_t PD>
KDTree<PT, PD>::KDTree(const std::vector<Point<PT, PD>> &points) {
    root_ = buildTree(points.begin(), points.end(), 0);
    size_ = points.size();       
}


template <typename PT, std::size_t PD>
std::optional<KDNode&> KDTree<PT, PD>::findNode(std::optional<KDNode&> currNode, const Point<PT, PD>& pt) const {
    if (!currNode.has_value() || currNode->point == pt) 
        return currNode;

    const Point<PT, PD>& currPoint = currNode->point;
    int currLevel = currNode->level;

    if (pt[currLevel % PD] < currPoint[currLevel % PD]) {
        return currNode->left.has_value() ? findNode(currNode->left, pt) : currNode;
    } else {
        return currNode->right.has_value() ? findNode(currNode->right, pt) : currNode;
    }
}

template <typename PT, std::size_t PD>
std::optional<KDNode&> KDTree<PT, PD>::findNode(const Point<PT, PD>& pt) const {
    return findNode(root_, pt);
}

template <typename PT, std::size_t PD>
void KDTree<PT, PD>::removeRecursively_internalCall(KDNode* currNode) {
    if (currNode == nullptr) 
        return;

    removeRecursively_internalCall(currNode->left);
    removeRecursively_internalCall(currNode->right);

    delete currNode;
}


template <typename PT, std::size_t PD>
void KDTree<PT, PD>::removeRecursively(KDNode* currNode) {
    removeRecursively_internalCall(currNode);
    size_t tmp = countNodes(root_); 
    if (getSize_t() != tmp)
        setSize_t(tmp);
}



template <typename PT, std::size_t PD>
void KDTree<PT, PD>::removeNode(std::optional<KDNode&> currNode){
    /*Non so bene come svilupparlo, se rimuovo un singolo nodo
    dovrei ricreare tutti i piani
    */
}

template <typename PT, std::size_t PD>
KDTree<PT, PD>::~KDTree() {
    freeResource(root_);
}

template <typename PT, std::size_t PD>
std::size_t KDTree<PT, PD>::dimension() const {
    return PD;
}

template <typename PT, std::size_t PD>
std::size_t KDTree<PT, PD>::countNodes(const KDNode* currNode) const {
    if (currNode == nullptr) {
        return 0;
    }
    return 1 + countNodes(currNode->left) + countNodes(currNode->right);
}


template <typename PT, std::size_t PD>
std::size_t KDTree<PT, PD>::countNodes() const {
    return countNodes(root_);
}

template <typename PT, std::size_t PD>
void KDTree<PT, PD>::setSize_t(size_t tmp){
    size_ = tmp;
}

template <typename PT, std::size_t PD>
size_t KDTree<PT, PD>::getSize_t(){
    return size_;
}

template <std::size_t N, typename ElemType>
void KDTree<N, ElemType>::insert(const Point<N>& pt, const ElemType& value) {
    // Caso 1: L'albero è vuoto
    if (root_ == nullptr) { 
        root_ = new KDNode(pt, value, 0); 
        size_ = 1;
        return;
    }

    // Caso 2: Cerca la posizione corretta per il nodo
    KDNode* current = root_;
    int level = 0;

    while (true) {
        if (current->point == pt) {
            current->value = value;
            return;
        }

        // Determina l'asse corrente
        int axis = level % N;

        if (pt[axis] < current->point[axis]) {
            // Inserisci a sinistra
            if (current->left == nullptr) {
                current->left = new KDNode(pt, value, level + 1);
                ++size_;
                return;
            } else {
                current = current->left;
            }
        } else {
            // Inserisci a destra
            if (current->right == nullptr) {
                current->right = new KDNode(pt, value, level + 1);
                ++size_;
                return;
            } else {
                current = current->right;
            }
        }

        ++level;
    }
}

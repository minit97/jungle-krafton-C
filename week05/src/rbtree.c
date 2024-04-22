#include "rbtree.h"
#include <stdio.h>
#include <stdlib.h>

rbtree *new_rbtree(void) {
  // 여러 개의 tree를 생성할 수 있어야 하며 각각 다른 내용들을 저장할 수 있어야 합니다.

  rbtree *p = (rbtree *)calloc(1, sizeof(rbtree));

  p->nil = (node_t *)calloc(1, sizeof(node_t));       
  p->root = p->nil;     // 가정 : 루트 노드의 부모는 nil로 한다.
  p->nil->color = RBTREE_BLACK;       
  
  return p;
}

// ================================================================================

void postorder_del(rbtree *t, node_t *pre) {   // 후위 순회
  if(pre == t->nil) {
    return;
  }
  // 서브트리 삭제 
  postorder_del(t, pre->left);
  postorder_del(t, pre->right);
  free(pre);
}

void delete_rbtree(rbtree *t) {               // 삭제(후위 순회)
  // 해당 tree가 사용했던 메모리를 전부 반환해야 합니다. (valgrind로 나타나지 않아야 함)

  postorder_del(t, t->root);
  free(t->nil);
  free(t);
}

// ================================================================================

void left_rotate(rbtree *t, node_t *x){      // 좌회전
  // t : rb-tree, target : 회전할 노드(자식이 될 노드)
  node_t *y = x->right;

  // y의 왼쪽 서브 트리를 x의 오른쪽 서브 트리로 옮긴다.
  x->right = y->left;
  if(y->left != t->nil) {
    y->left->parent = x;
  }
  
  // x의 부모를 y로 연결한다.
  y->parent = x->parent;
  if(x->parent == t->nil) {
    t->root = y;
  } else if (x == x->parent->left) {
    x->parent->left = y;
  } else {
    x->parent->right = y;
  }
  
  // x를 y의 왼쪽으로 놓는다.
  y->left = x;
  x->parent = y;
}

void right_rotate(rbtree *t, node_t *y){     // 우회전
  node_t *x = y->left;

  y->left = x->right;
  if(x->right != t->nil) {
    x->right->parent = y;
  }

  x->parent = y->parent;
  if (y->parent == t->nil) {
    t->root = x;
  } else if(y == y->parent->right) {
    y->parent->right = x;
  } else {
    y->parent->left = x;
  }
  
  x->right = y;
  y->parent = x;
}


void insert_fixup(rbtree *t, node_t *z) {      // RB-tree insert시 fixup
  while (z->parent->color == RBTREE_RED) {

    // z의 부모가 왼쪽 서브트리 
    if(z->parent == z->parent->parent->left) {
      node_t *uncle = z->parent->parent->right;

      // case 1 - recoloring 적용
      if(uncle->color == RBTREE_RED) {
        z->parent->color = RBTREE_BLACK;
        uncle->color = RBTREE_BLACK;
        z->parent->parent->color = RBTREE_RED;
        z = z->parent->parent;                 // recoloring 후 z 다시 세팅
      } else {
        // case 2 - restruturing을 위한 정렬
        if(z == z->parent->right) {
          z = z->parent;
          left_rotate(t, z);
        }

        // case 3 - restruturing 적용
        z->parent->color = RBTREE_BLACK;
        z->parent->parent->color = RBTREE_RED;
        right_rotate(t, z->parent->parent);
      }
    } else {  // z의 부모가 오른쪽 서브트리 
      node_t *uncle = z->parent->parent->left;

      // case 1
      if(uncle->color == RBTREE_RED) {
        z->parent->color = RBTREE_BLACK;
        uncle->color = RBTREE_BLACK;
        z->parent->parent->color = RBTREE_RED;
        z = z->parent->parent;
      } else {
        // case 2
        if(z == z->parent->left) {
          z = z->parent;
          right_rotate(t, z);
        }

        // case 3
        z->parent->color = RBTREE_BLACK;
        z->parent->parent->color = RBTREE_RED;
        left_rotate(t, z->parent->parent);
      }
    }
  }

  t->root->color = RBTREE_BLACK;
}

node_t *rbtree_insert(rbtree *t, const key_t key) {
  // 구현하는 ADT가 multiset이므로 이미 같은 key의 값이 존재해도 하나 더 추가 합니다.

  // 처음에 아무것도 없을 때
  if(t->root == t->nil) {
    t->root = (node_t *)calloc(1, sizeof(node_t));
    t->root->key = key;
    t->root->color = RBTREE_BLACK;
    t->root->left = t->root->right = t->root->parent = t->nil;
    return t->root;
  }

  // 추가 노드 선언
  node_t *new = (node_t *)calloc(1, sizeof(node_t));

  // key 노드 위치 찾기
  node_t *cur = t->root;
  while(1) {
    if(key <= cur->key) {   // 왼쪽 서브 트리
      if(cur->left != t->nil) {
        cur = cur->left;
      } else {
          cur->left = new;
          break;
      }
    } else {                // 오른쪽 서브 트리
      if(cur->right != t->nil) {
        cur = cur->right;
      } else {
        cur->right = new;
        break;
      }
    }
  }
  // 새로운 노드 설정
  new->color = RBTREE_RED;
  new->key = key;
  new->parent = cur;
  new->left = t->nil;
  new->right = t->nil;

  // Insert 마치고, fixup 진행 
  insert_fixup(t, new);
  return t->root;
}

// ================================================================================

node_t *rbtree_find(const rbtree *t, const key_t key) {
  // RB tree내에 해당 key가 있는지 탐색하여 있으면 해당 node pointer 반환
  // 해당하는 node가 없으면 NULL 반환

  node_t *target = t->root;
  while (target->key != key && target != t->nil) {
    if (target->key > key) {
      target = target->left;
    } else { 
      target = target->right;
    }
  }

  if (target == t->nil) {
    return NULL;
  }
  return target;
}

// ================================================================================

node_t *find_min(const rbtree *t, node_t *sub_root){  // 가장 왼쪽
  node_t *cur = sub_root;
  while (cur->left != t->nil) {
    cur = cur->left;
  }
  return cur;
}

node_t *rbtree_min(const rbtree *t) {
  // RB tree 중 최소 값을 가진 node pointer 반환

   return find_min(t, t->root);
}

// ================================================================================

node_t *rbtree_max(const rbtree *t) { // 가장 오른쪽
  // 최대값을 가진 node pointer 반환

  node_t *cur = t->root;
  while (cur->right != t->nil) {
    cur = cur->right;
  }
  return cur;
}

// ================================================================================

void transplant(rbtree *t, node_t *del, node_t *replace){ // del 노드를 replace 노드로 교체 (정확히는 del노드는 때어냄)
  if(del->parent == t->nil) {
    t->root = replace;
  } else if(del == del->parent->left) {
    del->parent->left = replace;
  } else {
    del->parent->right = replace;
  }
  replace->parent = del->parent;
}

// RBt delete시 fixup 
void del_fixup(rbtree *t, node_t *target){
  while(target != t->root && target->color == RBTREE_BLACK) {
    // left child
    if(target == target->parent->left) {
      node_t *uncle = target->parent->right;

      // case 1 : 삼촌이 적색인 경우
      if(uncle->color == RBTREE_RED) {
        uncle->color = RBTREE_BLACK;
        target->parent->color = RBTREE_RED;
        left_rotate(t, target->parent);
        uncle = target->parent->right;
      }

      // case 2 : 올 블랙
      if(uncle->left->color == RBTREE_BLACK && uncle->right->color == RBTREE_BLACK) {
        uncle->color = RBTREE_RED;
        target = target->parent;
      } else {

        // case 3 : 삼촌 블랙 & 오른쪽 블랙 & 왼쪽 레드
        if(uncle->right->color == RBTREE_BLACK) {
          uncle->left->color = RBTREE_BLACK;
          uncle->color = RBTREE_RED;
          right_rotate(t, uncle);
          uncle = target->parent->right;
        }

        // case 4 : 삼촌이 블랙이고 오른쪽이 레드일 경우
        uncle->color = target->parent->color;
        target->parent->color = RBTREE_BLACK;
        uncle->right->color = RBTREE_BLACK;
        left_rotate(t, target->parent);
        target = t->root;
      }

    } else {  // right child
      node_t *uncle = target->parent->left;

      if(uncle->color == RBTREE_RED){
        // case 1
        uncle->color = RBTREE_BLACK;
        target->parent->color = RBTREE_RED;
        right_rotate(t, target->parent);
        uncle = target->parent->left;
      }

      // case 2
      if(uncle->right->color == RBTREE_BLACK && uncle->left->color == RBTREE_BLACK){
        uncle->color = RBTREE_RED;
        target = target->parent;
      } else {
        // case 3
        if(uncle->left->color == RBTREE_BLACK){
          uncle->right->color = RBTREE_BLACK;
          uncle->color = RBTREE_RED;
          left_rotate(t, uncle);
          uncle = target->parent->left;
        }

        // case 4
        uncle->color = target->parent->color;
        target->parent->color = RBTREE_BLACK;
        uncle->left->color = RBTREE_BLACK;
        right_rotate(t, target->parent);
        target = t->root;
      }
    }
  }
  target->color = RBTREE_BLACK;
}

int rbtree_erase(rbtree *t, node_t *p) {
  // RB tree 내부의 ptr로 지정된 node를 삭제하고 메모리 반환

  color_t original_color = p->color;
  node_t * replace;
  if(p->left == t->nil) {
    replace = p->right;
    transplant(t, p, p->right);
  } else if (p->right == t->nil) {
    replace = p->left;
    transplant(t, p, p->left);
  } else {  // 자녀가 둘일 경우
    node_t * successor = find_min(t, p->right); // 후임자
    original_color = successor->color;

    replace = successor->right;
    if(successor->parent == p) {
      replace->parent = successor;
    } else {
      transplant(t, successor, successor->right);
      successor->right = p->right;
      successor->right->parent = successor;
    }

    // 삭제노드와 후임자 변경
    transplant(t, p, successor);
    successor->left = p->left;
    successor->left->parent = successor;
    successor->color = p->color;
  }
  free(p);  

  if(original_color == RBTREE_BLACK) {
    del_fixup(t, replace);
  }

  return 0;
}

// ================================================================================

int inorder_search(const rbtree * t, node_t *p, int idx, key_t *arr, int n) {   // 중위 순회
  if(p == t->nil || idx >= n) {
    return idx;
  }
  // printf("%d ",p->key);
  idx = inorder_search(t, p->left, idx, arr, n);
  arr[idx++] = p->key;
  idx = inorder_search(t, p->right, idx, arr, n);
  return idx;
}

int rbtree_to_array(const rbtree *t, key_t *arr, const size_t n) {    // 오름차순으로 탐색해서 그 결과를 n만큼 반환
  // RB tree의 내용을 key 순서대로 주어진 array로 변환
  // array의 크기는 n으로 주어지며 tree의 크기가 n 보다 큰 경우에는 순서대로 n개 까지만 변환
  // array의 메모리 공간은 이 함수를 부르는 쪽에서 준비하고 그 크기를 n으로 알려줍니다.

  inorder_search(t, t->root, 0, arr, n);
  printf("\n");
  return 0;
}

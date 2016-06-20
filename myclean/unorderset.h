#ifndef UNORDERSET_H
#define UNORDERSET_H
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/unordered_set.hpp>
#include <boost/functional/hash.hpp>
#include <boost/pool/pool.hpp>
#include <boost/pool/object_pool.hpp>
#include <iostream>
#include "memory.h"

template <class T, int BucketsSize>
class UnorderSet;
template <class T, int BucketsSize>
std::ostream& operator<<(std::ostream&os, const UnorderSet<T, BucketsSize>&cache);

namespace intrusive = boost::intrusive;
template <class T>
class OrdersetValue;
template <class T>
std::ostream& operator<<(std::ostream&os, const OrdersetValue<T> &);
template <class T>
class OrdersetValue
{
  friend std::ostream& operator<< <>(std::ostream&os, const OrdersetValue<T> &);
  T data_;

public:
  explicit OrdersetValue(const T& data): data_(data)
  {
  }
  ~OrdersetValue()
  {
  }
  const T & get() const  { return data_;    }
  T & get()  { return data_;    }
  void set(const T &k) {data_ = k;    }

  //This class can be inserted in an intrusive unordered_set
  intrusive::unordered_set_member_hook<>   unordered_set_hook_;

  //Comparison operators
  friend bool operator==(const OrdersetValue<T> &a, const OrdersetValue<T> &b)
  {  return a.data_ == b.data_; }


  friend bool operator!=(const OrdersetValue<T> &a, const OrdersetValue<T> &b)
  {  return a.data_ != b.data_; }

  //The hash function
  friend std::size_t hash_value(const OrdersetValue<T> &i)
  {  return boost::hash<T>()(i.data_);  }
};

template <class T>
std::ostream& operator<< (std::ostream&os, const OrdersetValue<T> &b)
{
  os << b.data_ << "\n";
  return os;
}

template<class T , int BucketsSize = 901>
class UnorderSet
{
public:
  typedef T key_type;

private:
  //Definition of the intrusive list that will hold Node
  typedef OrdersetValue<T> Node;
  //Definition of the intrusive unordered_set that will hold Node
  typedef intrusive::member_hook
  < Node, intrusive::unordered_set_member_hook<>
  , &Node::unordered_set_hook_> MemberUsetOption;
  typedef intrusive::unordered_set
  < Node, MemberUsetOption> unordered_set_t;

  typename unordered_set_t::bucket_type buckets_[BucketsSize];
  unordered_set_t  unordered_set_;

  boost::object_pool<Node, AllocatorMallocFree> pool_;
public:
  typedef typename intrusive::unordered_set < Node, MemberUsetOption>::iterator iterator;
  iterator begin()
  {
    return unordered_set_.begin();
  }

  iterator end()
  {
    return unordered_set_.end();
  }

  friend std::ostream& operator<< <>(std::ostream&os, const UnorderSet<T, BucketsSize>&);

  UnorderSet(): unordered_set_(typename unordered_set_t::bucket_traits(buckets_, BucketsSize)) {}
  T* find(const T& id)
  {
    typename unordered_set_t::iterator unordered_set_it = unordered_set_.find(Node(id));

    if (unordered_set_it == unordered_set_.end()) {
      return 0;
    } else {
      return &unordered_set_it->get();
    }
  }

  T* insert(const T& id)
  {
    Node node(id);
    typename unordered_set_t::iterator unordered_set_it = unordered_set_.find(node);

    if (unordered_set_it != unordered_set_.end()) {
      return & unordered_set_it->get();
    } else {
      Node * pnode = pool_.construct(id);
      unordered_set_.insert(*pnode);
      return & pnode->get();
    }
  }

  bool erase(const T& id)
  {
    typename unordered_set_t::iterator unordered_set_it = unordered_set_.find(Node(id));

    if (unordered_set_it != unordered_set_.end()) {
      unordered_set_.erase(*unordered_set_it);
      pool_.destroy(& (*unordered_set_it));
      return true;
    } else {
      return false;
    }
  }

  bool erase_n(int n)
  {
    if (n > 0) {
      for (int i = 0; i < n; ++i) {
        if (!pop_front()) {
          return false;
        }
      }
    }

    return true;
  }

  bool pop_front()
  {
    if (unordered_set_.empty()) {
      return false;
    } else {
      Node* p = &(*unordered_set_.begin());
      unordered_set_.erase(unordered_set_.begin());
      pool_.destroy(p);
      return true;
    }
  }

  void clear()
  {
    while (pop_front()) {}
  }

  ~UnorderSet() {clear();}

};
template<class T, int BucketsSize>
std::ostream& operator<<(std::ostream&os, const UnorderSet<T, BucketsSize>&cache)
{
  typedef OrdersetValue<T> ID_Attribute;
  typename UnorderSet<T, BucketsSize>::unordered_set_t::const_iterator it = cache.unordered_set_.begin();
  typename UnorderSet<T, BucketsSize>::unordered_set_t::const_iterator ite = cache.unordered_set_.end();

  for (; it != ite; ++it) {
    std::cout << *it;
  }

  return os;
}
#endif
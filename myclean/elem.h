#ifndef ELEM_H
#define ELEM_H
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/unordered_set.hpp>
#include <boost/functional/hash.hpp>
#include <boost/pool/pool.hpp>
#include <boost/pool/object_pool.hpp>
#include <iostream>
namespace intrusive = boost::intrusive;

template <class T>
class Elem;

template <class T>
std::ostream& operator<<(std::ostream&os, const Elem<T> &);

template <class T>
class Elem
{
  friend std::ostream& operator<< <>(std::ostream&os, const Elem<T> &);
  T data_;

public:
  explicit Elem(const T& data): data_(data)
  {
  }
  template <class A1, class A2, class A3>
  Elem(const A1& a, const A2 &a2, const A3& a3):data_(a, a2, a3){

  }
  template <class A1, class A2>
  Elem(const A1& a, const A2 &a2):data_(a, a2){

  }
  template <class A>
  Elem(const A& a):data_(a){

  }
  const T & get() const  { return data_;    }
  T & get()  { return data_;    }
  void set(const T &k) {data_ = k;    }
  bool canEraseNow()const {canErase(data_); }

  //This class can be inserted in an intrusive list
  intrusive::list_member_hook<>   list_hook_;

  //This class can be inserted in an intrusive unordered_set
  intrusive::unordered_set_member_hook<>   unordered_set_hook_;

  //Comparison operators
  friend bool operator==(const Elem<T> &a, const Elem<T> &b)
  {  return a.data_ == b.data_; }


  friend bool operator!=(const Elem<T> &a, const Elem<T> &b)
  {  return a.data_ != b.data_; }

  //The hash function
  friend std::size_t hash_value(const Elem<T> &i)
  {  return boost::hash<T>()(i.data_);  }
};

template <class T>
std::ostream& operator<< (std::ostream&os, const Elem<T> &b)
{
  os << b.data_ << "\n";
  return os;
}

#endif
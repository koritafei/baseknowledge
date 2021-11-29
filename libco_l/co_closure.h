#ifndef __CO_CLOSURE_H__
#define __CO_CLOSURE_H__

struct stCoclosure_t {
public:
  virtual void exec() = 0;
};

// comac_arg
#define comac_get_args_cnt(...) comac_arg_n(__VA_ARGS__)
#define comac_arg_n(_0, _1, _2, _3, _4, _5, _6, _7, N, ...) N
#define comac_args_seqs() 7, 6, 5, 4, 3, 2, 1, 0
#define comac_join_1(x, y) (x##y)

#define comac_argc(...) comac_get_args_cnt(0, ##__VA_ARGS__, comac_arg_seqs())

// replace
#define repeat_0(func, a, ...)
#define repeat_1(func, a, ...)                                                 \
  func(1, a, __VA_ARGS__) repeat_0(func, __VA_ARGS__)
#define repeat_2(func, a, ...)                                                 \
  func(2, a, __VA_ARGS__) repeat_1(func, __VA_ARGS__)
#define repeat_3(func, a, ...)                                                 \
  func(2, a, __VA_ARGS__) repeat_2(func, __VA_ARGS__)
#define repeat_4(func, a, ...)                                                 \
  func(2, a, __VA_ARGS__) repeat_3(func, __VA_ARGS__)
#define repeat_5(func, a, ...)                                                 \
  func(2, a, __VA_ARGS__) repeat_4(func, __VA_ARGS__)
#define repeat_6(func, a, ...)                                                 \
  func(2, a, __VA_ARGS__) repeat_5(func, __VA_ARGS__)

#define repeat(n, fun, ...) comac_join(repeat_, n)(fun, __VA_ARGS__)

// implemenmt
#if __cplusplus <= 199711L
#define decl_typeof(i, a, ...) typedef typeof(a) typeof_##a;
#else
#define decl_typeof(i, a, ...) typedef decltype(a) typeof_##a;
#endif
#define impl_typeof(i, a, ...) typeof_##a &a;
#define impl_typeof_cpy(i, a, ...) typeof_##a a;
#define con_param_typeof(i, a, ...) typeof_##a &a##r,
#define param_init_typeof(i, a, ...) a(a##r),

// reference
#define co_ref(name, ...)                                                      \
  repeat(comac_argc(__VA_ARGS__), decl_typeof,                                 \
         __VA_ARGS__) class type_##name {                                      \
  public:                                                                      \
    repeat(comac_argc(__VA_ARGS__), impl_typeof, __VA_ARGS__) int _member_cnt; \
    type_##name(repeat(comac_argc(__VA_ARGS__), con_param_typeof,              \
                       __VA_ARGS__)...)                                        \
        : repeat(comac_argc(__VA_ARGS__), param_init_typeof, __VA_ARGS__)      \
              _member_cnt(comac_argc(__VA_ARGS__)) {}                          \
  } name(__VA_ARGS__);

// 2.2 function

#define co_func(name, ...)                                                     \
  repeat(comac_argc(__VA_ARGS__), decl_typeof, __VA_ARGS__) class name         \
      : public stCoClosure_t {                                                 \
  public:                                                                      \
    repeat(comac_argc(__VA_ARGS__), impl_typeof_cpy,                           \
           __VA_ARGS__) int _member_cnt;                                       \
                                                                               \
  public:                                                                      \
    name(repeat(comac_argc(__VA_ARGS__), con_param_typeof, __VA_ARGS__)...)    \
        : repeat(comac_argc(__VA_ARGS__), param_init_typeof, __VA_ARGS__)      \
              _member_cnt(comac_argc(__VA_ARGS__)) {}                          \
    void exec()

#define co_func_end }

#endif /* __CO_CLOSURE_H__ */

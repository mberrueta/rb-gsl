/////////////////////////////////////////////////////////////////////
// = NMatrix
//
// A linear algebra library for scientific computation in Ruby.
// NMatrix is part of SciRuby.
//
// NMatrix was originally inspired by and derived from NArray, by
// Masahiro Tanaka: http://narray.rubyforge.org
//
// == Copyright Information
//
// SciRuby is Copyright (c) 2010 - 2012, Ruby Science Foundation
// NMatrix is Copyright (c) 2012, Ruby Science Foundation
//
// Please see LICENSE.txt for additional copyright notices.
//
// == Contributing
//
// By contributing source code to SciRuby, you agree to be bound by
// our Contributor Agreement:
//
// * https://github.com/SciRuby/sciruby/wiki/Contributor-Agreement
//
// == nmatrix.c
//

/////////////////////////////////////////////////////////////////////
// = NMatrix
//
// A linear algebra library for scientific computation in Ruby.
// NMatrix is part of SciRuby.
//
// NMatrix was originally inspired by and derived from NArray, by
// Masahiro Tanaka: http://narray.rubyforge.org
//
// == Copyright Information
//
// SciRuby is Copyright (c) 2010 - 2012, Ruby Science Foundation
// NMatrix is Copyright (c) 2012, Ruby Science Foundation
//
// Please see LICENSE.txt for additional copyright notices.
//
// == Contributing
//
// By contributing source code to SciRuby, you agree to be bound by
// our Contributor Agreement:
//
// * https://github.com/SciRuby/sciruby/wiki/Contributor-Agreement
//
// == data.cpp
//
// Functions and data for dealing the data types.

/*
 * Standard Includes
 */

#include <ruby.h>

/*
 * Project Includes
 */

#include "nmatrix.h"
#include "storage/storage.h"
#include "ruby_constants.h"

/*
 * Macros
 */

typedef VALUE (*METHOD)(...);

/*
 * Global Variables
 */

/*
 * Forward Declarations
 */

static VALUE nm_init(int argc, VALUE* argv, VALUE nm);
static VALUE nm_init_copy(VALUE copy, VALUE original);
static VALUE nm_init_cast_copy(VALUE copy, VALUE original, VALUE new_stype, VALUE new_dtype);
static VALUE nm_init_yale_from_old_yale(VALUE shape, VALUE dtype, VALUE ia, VALUE ja, VALUE a, VALUE from_dtype, VALUE nm);
static VALUE nm_alloc(VALUE klass);
static void  nm_delete(NMATRIX* mat);
static void  nm_delete_ref(NMATRIX* mat);
static VALUE nm_dtype(VALUE self);
static VALUE nm_itype(VALUE self);
static VALUE nm_stype(VALUE self);
static VALUE nm_rank(VALUE self);
static VALUE nm_shape(VALUE self);
static VALUE nm_capacity(VALUE self);
static VALUE nm_each(VALUE nmatrix);

static SLICE* get_slice(size_t rank, VALUE* c, VALUE self);
static VALUE nm_xslice(int argc, VALUE* argv, void* (*slice_func)(STORAGE*, SLICE*), void (*delete_func)(NMATRIX*), VALUE self);
static VALUE nm_mset(int argc, VALUE* argv, VALUE self);
static VALUE nm_mget(int argc, VALUE* argv, VALUE self);
static VALUE nm_mref(int argc, VALUE* argv, VALUE self);
static VALUE nm_is_ref(VALUE self);

static VALUE is_symmetric(VALUE self, bool hermitian);

static VALUE nm_symmetric(VALUE self);
static VALUE nm_hermitian(VALUE self);

static VALUE nm_eqeq(VALUE left, VALUE right);

static VALUE matrix_multiply_scalar(NMATRIX* left, VALUE scalar);
static VALUE matrix_multiply(NMATRIX* left, NMATRIX* right);
static VALUE nm_multiply(VALUE left_v, VALUE right_v);

static dtype_t	dtype_from_rbstring(VALUE str);
static dtype_t	dtype_from_rbsymbol(VALUE sym);
static itype_t  itype_from_rbsymbol(VALUE sym);
static dtype_t	dtype_guess(VALUE v);
static dtype_t	interpret_dtype(int argc, VALUE* argv, stype_t stype);
static void*		interpret_initial_value(VALUE arg, dtype_t dtype);
static size_t*	interpret_shape(VALUE arg, size_t* rank);
static stype_t	interpret_stype(VALUE arg);
static stype_t	stype_from_rbstring(VALUE str);
static stype_t	stype_from_rbsymbol(VALUE sym);

#ifdef DEBUG_YALE
/* Yale-specific functions */
static VALUE nm_yale_size(VALUE self);
static VALUE nm_yale_a(VALUE self);
static VALUE nm_yale_d(VALUE self);
static VALUE nm_yale_lu(VALUE self);
static VALUE nm_yale_ia(VALUE self);
static VALUE nm_yale_ja(VALUE self);
static VALUE nm_yale_ija(VALUE self);
#endif

#ifdef BENCHMARK
static double get_time(void);
#endif

/*
 * Functions
 */

///////////////////
// Ruby Bindings //
///////////////////

extern "C" {

void Init_nmatrix() {
	
	///////////////////////
	// Class Definitions //
	///////////////////////
	
	cNMatrix = rb_define_class("NMatrix", rb_cObject);
	cNVector = rb_define_class("NVector", cNMatrix);
	
	// Special exceptions
	nm_eDataTypeError    = rb_define_class("DataTypeError",			rb_eStandardError);
	nm_eStorageTypeError = rb_define_class("StorageTypeError",	rb_eStandardError);

	///////////////////
	// Class Methods //
	///////////////////
	
	rb_define_alloc_func(cNMatrix, nm_alloc);
	
	/*
	 * FIXME: These need to be bound in a better way.
	rb_define_singleton_method(cNMatrix, "__cblas_gemm__", nm_cblas_gemm, 13);
	rb_define_singleton_method(cNMatrix, "__cblas_gemv__", nm_cblas_gemv, 11);
	rb_define_singleton_method(cNMatrix, "upcast", nm_upcast, 2);
	 */
	
	//////////////////////
	// Instance Methods //
	//////////////////////

	rb_define_method(cNMatrix, "initialize", (METHOD)nm_init, -1);
	rb_define_method(cNMatrix, "initialize_copy", (METHOD)nm_init_copy, 1);

	//rb_define_method(cNMatrix, "initialize_cast_copy", (METHOD)nm_init_cast_copy, 2);
	//rb_define_method(cNMatrix, "as_dtype", (METHOD)nm_cast_copy, 1);
	
	rb_define_method(cNMatrix, "dtype", (METHOD)nm_dtype, 0);
	rb_define_method(cNMatrix, "itype", (METHOD)nm_itype, 0);
	rb_define_method(cNMatrix, "stype", (METHOD)nm_stype, 0);
	rb_define_method(cNMatrix, "cast",  (METHOD)nm_init_cast_copy, 2);

	rb_define_method(cNMatrix, "[]", (METHOD)nm_mref, -1);
	rb_define_method(cNMatrix, "slice", (METHOD)nm_mget, -1);
	rb_define_method(cNMatrix, "[]=", (METHOD)nm_mset, -1);
	rb_define_method(cNMatrix, "is_ref?", (METHOD)nm_is_ref, 0);
	rb_define_method(cNMatrix, "rank", (METHOD)nm_rank, 0);
	rb_define_method(cNMatrix, "shape", (METHOD)nm_shape, 0);
	//rb_define_method(cNMatrix, "transpose", (METHOD)nm_transpose_new, 0);   // FIXME 0.0.2
	//rb_define_method(cNMatrix, "det_exact", (METHOD)nm_det_exact, 0);       // FIXME 0.0.2
	//rb_define_method(cNMatrix, "transpose!", (METHOD)nm_transpose_self, 0); // FIXME 0.0.2
	//rb_define_method(cNMatrix, "complex_conjugate!", (METHOD)nm_complex_conjugate_bang, 0); // FIXME 0.0.2

	rb_define_method(cNMatrix, "each", (METHOD)nm_each, 0);

  // FIXME 0.0.2
	//rb_define_method(cNMatrix, "*", (METHOD)nm_ew_multiply, 1);
	//rb_define_method(cNMatrix, "/", (METHOD)nm_ew_divide, 1);
	//rb_define_method(cNMatrix, "+", (METHOD)nm_ew_add, 1);
	//rb_define_method(cNMatrix, "-", (METHOD)nm_ew_subtract, 1);
	//rb_define_method(cNMatrix, "%", (METHOD)nm_ew_mod, 1);
	rb_define_method(cNMatrix, "eql?", (METHOD)nm_eqeq, 1);
	rb_define_method(cNMatrix, "dot", (METHOD)nm_multiply, 1);
	
	/*
	 * TODO: Write new elementwise code for boolean operations
	rb_define_method(cNMatrix, "==", (METHOD)nm_ew_eqeq, 1);
	rb_define_method(cNMatrix, "!=", (METHOD)nm_ew_neq, 1);
	rb_define_method(cNMatrix, "<=", (METHOD)nm_ew_leq, 1);
	rb_define_method(cNMatrix, ">=", (METHOD)nm_ew_geq, 1);
	rb_define_method(cNMatrix, "<", (METHOD)nm_ew_lt, 1);
	rb_define_method(cNMatrix, ">", (METHOD)nm_ew_gt, 1);
	 */

	rb_define_method(cNMatrix, "symmetric?", (METHOD)nm_symmetric, 0);
	rb_define_method(cNMatrix, "hermitian?", (METHOD)nm_hermitian, 0);

	rb_define_method(cNMatrix, "capacity", (METHOD)nm_capacity, 0);

#ifdef DEBUG_YALE
	rb_define_method(cNMatrix, "__yale_ija__", (METHOD)nm_yale_ija, 0);
	rb_define_method(cNMatrix, "__yale_a__", (METHOD)nm_yale_a, 0);
	rb_define_method(cNMatrix, "__yale_size__", (METHOD)nm_yale_size, 0);
	rb_define_method(cNMatrix, "__yale_ia__", (METHOD)nm_yale_ia, 0);
	rb_define_method(cNMatrix, "__yale_ja__", (METHOD)nm_yale_ja, 0);
	rb_define_method(cNMatrix, "__yale_d__", (METHOD)nm_yale_d, 0);
	rb_define_method(cNMatrix, "__yale_lu__", (METHOD)nm_yale_lu, 0);
	//rb_define_const(cNMatrix, "YALE_GROWTH_CONSTANT", rb_float_new(YALE_GROWTH_CONSTANT));
#endif
	
	/////////////
	// Aliases //
	/////////////
	
	rb_define_alias(cNMatrix, "dim", "rank");
	rb_define_alias(cNMatrix, "equal?", "eql?");
	
	///////////////////////
	// Symbol Generation //
	///////////////////////
	
	ruby_constants_init();
}

/*
 * End of the Ruby binding functions in the `extern "C" {}` block.
 */
}

//////////////////
// Ruby Methods //
//////////////////

/*
 * Allocator.
 */
static VALUE nm_alloc(VALUE klass) {
  NMATRIX* mat = ALLOC(NMATRIX);
  mat->storage = NULL;
  // FIXME: mark_table[mat->stype] should be passed to Data_Wrap_Struct, but can't be done without stype. Also, nm_delete depends on this.
  // mat->stype   = NUM_STYPES;

  //STYPE_MARK_TABLE(mark_table);

  return Data_Wrap_Struct(klass, NULL, nm_delete, mat);
}

/*
 * Find the capacity of an NMatrix. The capacity only differs from the size for
 * Yale matrices, which occasionally allocate more space than they need. For
 * list and dense, capacity gives the number of elements in the matrix.
 */
static VALUE nm_capacity(VALUE self) {
  VALUE cap;

  switch(NM_STYPE(self)) {
  case YALE_STORE:
    cap = UINT2NUM(((YALE_STORAGE*)(NM_STORAGE(self)))->capacity);
    break;

  case DENSE_STORE:
    cap = UINT2NUM(storage_count_max_elements( NM_DENSE_STORAGE(self) ));
    break;

  case LIST_STORE:
    cap = UINT2NUM(list_storage_count_elements( NM_LIST_STORAGE(self) ));
    break;

  default:
    rb_raise(nm_eStorageTypeError, "unrecognized stype in nm_capacity()");
  }

  return cap;
}

/*
 * Destructor.
 */
static void nm_delete(NMATRIX* mat) {
  static void (*ttable[NUM_STYPES])(STORAGE*) = {
    dense_storage_delete,
    list_storage_delete,
    yale_storage_delete
  };
  ttable[mat->stype](mat->storage);
}

/*
 * Slicing destructor.
 */
static void nm_delete_ref(NMATRIX* mat) {
  static void (*ttable[NUM_STYPES])(STORAGE*) = {
    dense_storage_delete_ref,
    list_storage_delete,  // FIXME: Should these be _ref?
    yale_storage_delete
  };
  ttable[mat->stype](mat->storage);
}

/*
 * Get the data type (dtype) of a matrix, e.g., :byte, :int8, :int16, :int32,
 * :int64, :float32, :float64, :complex64, :complex128, :rational32,
 * :rational64, :rational128, or :object (the last is a Ruby object).
 */
static VALUE nm_dtype(VALUE self) {
  ID dtype = rb_intern(DTYPE_NAMES[NM_DTYPE(self)]);
  return ID2SYM(dtype);
}


/*
 * Get the index data type (dtype) of a matrix. Defined only for yale; others return nil.
 */
static VALUE nm_itype(VALUE self) {
  if (NM_STYPE(self) == YALE_STORE) {
    ID itype = rb_intern(ITYPE_NAMES[NM_ITYPE(self)]);
    return ID2SYM(itype);
  }
  return Qnil;
}


/*
 * Borrowed this function from NArray. Handles 'each' iteration on a dense
 * matrix.
 *
 * Additionally, handles separately matrices containing VALUEs and matrices
 * containing other types of data.
 */
static VALUE nm_each_dense(VALUE nmatrix) {
  DENSE_STORAGE* s = NM_DENSE_STORAGE(nmatrix);
  VALUE v;
  size_t i;

  if (NM_DTYPE(nmatrix) == RUBYOBJ) {

    // matrix of Ruby objects -- yield those objects directly
    for (i = 0; i < storage_count_max_elements(s); ++i)
      rb_yield( *((VALUE*)((char*)(s->elements) + i*DTYPE_SIZES[NM_DTYPE(nmatrix)])) );

  } else {
    // We're going to copy the matrix element into a Ruby VALUE and then operate on it. This way user can't accidentally
    // modify it and cause a seg fault.

    for (i = 0; i < storage_count_max_elements(s); ++i) {
      v = rubyobj_from_cval((char*)(s->elements) + i*DTYPE_SIZES[NM_DTYPE(nmatrix)], NM_DTYPE(nmatrix)).rval;
      rb_yield(v); // yield to the copy we made
    }
  }

  return nmatrix;
}


/*
 * Iterate over the matrix as you would an Enumerable (e.g., Array).
 *
 * Currently only works for dense.
 */
static VALUE nm_each(VALUE nmatrix) {
  volatile VALUE nm = nmatrix; // not sure why we do this, but it gets done in ruby's array.c.

  switch(NM_STYPE(nm)) {
  case DENSE_STORE:
    return nm_each_dense(nm);
  default:
    rb_raise(rb_eNotImpError, "only dense matrix's each method works right now");
  }
}



/*
 * Equality operator. Returns a single true or false value indicating whether
 * the matrices are equivalent.
 *
 * For elementwise, use == instead.
 *
 * This method will raise an exception if dimensions do not match.
 */
static VALUE nm_eqeq(VALUE left, VALUE right) {
  NMATRIX *l, *r;

  CheckNMatrixType(left);
  CheckNMatrixType(right);

  UnwrapNMatrix(left, l);
  UnwrapNMatrix(right, r);

  if (l->stype != r->stype)
    rb_raise(rb_eNotImpError, "comparison between different matrix stypes not yet implemented");

  bool result = false;

  switch(l->stype) {
  case DENSE_STORE:
    result = dense_storage_eqeq(l->storage, r->storage);
    break;
  case LIST_STORE:
    result = list_storage_eqeq(l->storage, r->storage);
    break;
  case YALE_STORE:
    result = yale_storage_eqeq(l->storage, r->storage);
    break;
  }

  return result ? Qtrue : Qfalse;
}

/*
 * Is this matrix hermitian?
 *
 * Definition: http://en.wikipedia.org/wiki/Hermitian_matrix
 *
 * For non-complex matrices, this function should return the same result as symmetric?.
 */
static VALUE nm_hermitian(VALUE self) {
  return is_symmetric(self, true);
}


/*
 * Helper function for creating a matrix. You have to create the storage and pass it in, but you don't
 * need to worry about deleting it.
 */
NMATRIX* nm_create(stype_t stype, STORAGE* storage) {
  NMATRIX* mat = ALLOC(NMATRIX);

  mat->stype   = stype;
  mat->storage = storage;

  return mat;
}

/*
 * Create a new NMatrix.
 *
 * There are several ways to do this. At a minimum, dimensions and either a dtype or initial values are needed, e.g.,
 *
 *     NMatrix.new(3, :int64)       # square 3x3 dense matrix
 *     NMatrix.new([3,4], :float32) # 3x4 matrix
 *     NMatrix.new(3, 0)            # 3x3 dense matrix initialized to all zeros
 *     NMatrix.new([3,3], [1,2,3])  # [[1,2,3],[1,2,3],[1,2,3]]
 *
 * NMatrix will try to guess the dtype from the first value in the initial values array.
 *
 * You can also provide the stype prior to the dimensions. However, non-dense matrices cannot take initial values, and
 * require a dtype (e.g., :int64):
 *
 *     NMatrix.new(:yale, [4,3], :int64)
 *     NMatrix.new(:list, 5, :rational128)
 *
 * For Yale, you can also give an initial size for the non-diagonal component of the matrix:
 *
 *     NMatrix.new(:yale, [4,3], 2, :int64)
 *
 * Finally, you can be extremely specific, and define a matrix very exactly:
 *
 *     NMatrix.new(:dense, [2,2,2], [0,1,2,3,4,5,6,7], :int8)
 *
 * There is one additional constructor for advanced users, which takes seven arguments and is only for creating Yale matrices
 * with known IA, JA, and A arrays. This is used primarily internally for IO, e.g., reading Matlab matrices, which are
 * stored in old Yale format.
 *
 * Just be careful! There are no overflow warnings in NMatrix.
 */
static VALUE nm_init(int argc, VALUE* argv, VALUE nm) {

  if (argc < 2) {
  	rb_raise(rb_eArgError, "Expected 2-4 arguments (or 7 for internal Yale creation)");
  	return Qnil;
  }

  /* First, determine stype (dense by default) */
  stype_t stype;
  size_t  offset = 0;

  if (!SYMBOL_P(argv[0]) && !RUBYVAL_IS_STRING(argv[0])) {
    stype = DENSE_STORE;
    
  } else {
  	// 0: String or Symbol
    stype  = interpret_stype(argv[0]);
    offset = 1;
  }

  // If there are 7 arguments and Yale, refer to a different init function with fewer sanity checks.
  if (argc == 7) {
    if (stype == YALE_STORE) {
    	return nm_init_yale_from_old_yale(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], nm);
    	
		} else {
		  rb_raise(rb_eArgError, "Expected 2-4 arguments (or 7 for internal Yale creation)");
		  return Qnil;
		}
  }
	
	// 1: Array or Fixnum
	size_t rank;
  size_t* shape = interpret_shape(argv[offset], &rank);

  // 2-3: dtype
  dtype_t dtype = interpret_dtype(argc-1-offset, argv+offset+1, stype);

  size_t init_cap = 0, init_val_len = 0;
  void* init_val  = NULL;
  if (RUBYVAL_IS_NUMERIC(argv[1+offset]) || TYPE(argv[1+offset]) == T_ARRAY) {
  	// Initial value provided (could also be initial capacity, if yale).
  	
    if (stype == YALE_STORE) {
      init_cap = FIX2UINT(argv[1+offset]);
      
    } else {
    	// 4: initial value / dtype
      init_val = interpret_initial_value(argv[1+offset], dtype);
      
      if (TYPE(argv[1+offset]) == T_ARRAY) 	init_val_len = RARRAY_LEN(argv[1+offset]);
      else                                  init_val_len = 1;
    }
    
  } else {
  	// DType is RUBYOBJ.
  	
    if (stype == DENSE_STORE) {
    	/*
    	 * No need to initialize dense with any kind of default value unless it's
    	 * an RUBYOBJ matrix.
    	 */
      if (dtype == RUBYOBJ) {
      	// Pretend [nil] was passed for RUBYOBJ.
      	init_val = ALLOC(VALUE);
        *(VALUE*)init_val = Qnil;
        
        init_val_len = 1;
        
      } else {
      	init_val = NULL;
      }
    } else if (stype == LIST_STORE) {
    	init_val = ALLOC_N(char, DTYPE_SIZES[dtype]);
      memset(init_val, 0, DTYPE_SIZES[dtype]);
    }
  }
	
  // TODO: Update to allow an array as the initial value.
	NMATRIX* nmatrix;
  UnwrapNMatrix(nm, nmatrix);

  nmatrix->stype = stype;
  
  switch (stype) {
  	case DENSE_STORE:
  		nmatrix->storage = (STORAGE*)dense_storage_create(dtype, shape, rank, init_val, init_val_len);
  		break;
  		
  	case LIST_STORE:
  		nmatrix->storage = (STORAGE*)list_storage_create(dtype, shape, rank, init_val);
  		break;
  		
  	case YALE_STORE:
  		nmatrix->storage = (STORAGE*)yale_storage_create(dtype, shape, rank, init_cap);
  		yale_storage_init((YALE_STORAGE*)(nmatrix->storage));
  		break;
  }

  return nm;
}

/*
 * Copy constructor for changing dtypes and stypes.
 */
static VALUE nm_init_cast_copy(VALUE copy, VALUE original, VALUE new_stype_symbol, VALUE new_dtype_symbol) {
  NMATRIX *lhs, *rhs;

  dtype_t new_dtype = dtype_from_rbsymbol(new_dtype_symbol);
  stype_t new_stype = stype_from_rbsymbol(new_stype_symbol);

  CheckNMatrixType(original);

  if (copy == original) return copy;

  UnwrapNMatrix( original, rhs );
  UnwrapNMatrix( copy,     lhs );

  lhs->stype = new_stype;

  // Copy the storage
  STYPE_CAST_COPY_TABLE(ttable);
  lhs->storage = ttable[new_stype][rhs->stype](rhs->storage, new_dtype);

  return copy;
}


/*
 * Copy constructor for no change of dtype or stype (used for #initialize_copy hook).
 */
static VALUE nm_init_copy(VALUE copy, VALUE original) {
  NMATRIX *lhs, *rhs;

  CheckNMatrixType(original);

  if (copy == original) return copy;

  UnwrapNMatrix( original, rhs );
  UnwrapNMatrix( copy,     lhs );

  lhs->stype = rhs->stype;

  // Copy the storage
  STYPE_CAST_COPY_TABLE(ttable);
  lhs->storage = ttable[lhs->stype][rhs->stype](rhs->storage, rhs->storage->dtype);

  return copy;
}


/*
 * Create a new NMatrix helper for handling internal ia, ja, and a arguments.
 *
 * This constructor is only called by Ruby code, so we can skip most of the
 * checks.
 */
static VALUE nm_init_yale_from_old_yale(VALUE shape, VALUE dtype, VALUE ia, VALUE ja, VALUE a, VALUE from_dtype, VALUE nm) {
  size_t rank     = 2;
  size_t* shape_  = interpret_shape(shape, &rank);
  dtype_t dtype_  = dtype_from_rbsymbol(dtype);
  char *ia_       = RSTRING_PTR(ia),
       *ja_       = RSTRING_PTR(ja),
       *a_        = RSTRING_PTR(a);
  dtype_t from_dtype_ = dtype_from_rbsymbol(from_dtype);
  NMATRIX* nmatrix;

  UnwrapNMatrix( nm, nmatrix );

  nmatrix->stype   = YALE_STORE;
  nmatrix->storage = (STORAGE*)yale_storage_create_from_old_yale(dtype_, shape_, ia_, ja_, a_, from_dtype_);

  return nm;
}

/*
 * Check to determine whether matrix is a reference to another matrix.
 */
static VALUE nm_is_ref(VALUE self) {
	// Refs only allowed for dense matrices.
  if (NM_STYPE(self) == DENSE_STORE) {
    return (NM_DENSE_SRC(self) == NM_STORAGE(self)) ? Qfalse : Qtrue;
  }

  return Qfalse;
}



/*
 * Access the contents of an NMatrix at given coordinates, using copying.
 *
 *     n.slice(3,3)  # => 5.0
 *     n.slice(0..1,0..1) #=> matrix [2,2]
 *
 */
static VALUE nm_mget(int argc, VALUE* argv, VALUE self) {
  static void* (*ttable[NUM_STYPES])(STORAGE*, SLICE*) = {
    dense_storage_get,
    list_storage_get,
    yale_storage_get
  };
  
  return nm_xslice(argc, argv, ttable[NM_STYPE(self)], nm_delete, self);
}

/*
 * Access the contents of an NMatrix at given coordinates by reference.
 *
 *     n[3,3]  # => 5.0
 *     n[0..1,0..1] #=> matrix [2,2]
 *
 */
static VALUE nm_mref(int argc, VALUE* argv, VALUE self) {
  static void* (*ttable[NUM_STYPES])(STORAGE*, SLICE*) = {
    dense_storage_ref,
    list_storage_ref,
    yale_storage_ref
  };
  return nm_xslice(argc, argv, ttable[NM_STYPE(self)], nm_delete, self);
}

/*
 * Modify the contents of an NMatrix in the given cell
 *
 *     n[3,3] = 5.0
 *
 * Also returns the new contents, so you can chain:
 *
 *     n[3,3] = n[2,3] = 5.0
 */
static VALUE nm_mset(int argc, VALUE* argv, VALUE self) {
  size_t rank = argc - 1; // last arg is the value

  if (argc <= 1) {
    rb_raise(rb_eArgError, "Expected coordinates and r-value");

  } else if (NM_RANK(self) == rank) {

    SLICE* slice = get_slice(rank, argv, self);

    void* value = rubyobj_to_cval(argv[rank], NM_DTYPE(self));

    // FIXME: Can't use a function pointer table here currently because these functions have different
    // signatures (namely the return type).
    switch(NM_STYPE(self)) {
    case DENSE_STORE:
      dense_storage_set(NM_STORAGE(self), slice, value);
      break;
    case LIST_STORE:
      // Remove if it's a zero, insert otherwise
      if (!memcmp(value, NM_LIST_STORAGE(self)->default_val, DTYPE_SIZES[NM_DTYPE(self)])) {
        free(value);
        value = list_storage_remove(NM_STORAGE(self), slice);
        free(value);
      } else {
        list_storage_insert(NM_STORAGE(self), slice, value);
      }
      break;
    case YALE_STORE:
      yale_storage_set(NM_STORAGE(self), slice, value);
      break;
    }

    return argv[rank];

  } else if (NM_RANK(self) < rank) {
    rb_raise(rb_eArgError, "Coordinates given exceed matrix rank");
  } else {
    rb_raise(rb_eNotImpError, "Slicing not supported yet");
  }
  return Qnil;
}

/*
 * Matrix multiply (dot product): against another matrix or a vector.
 *
 * For elementwise, use * instead.
 *
 * The two matrices must be of the same stype (for now). If dtype differs, an upcast will occur.
 */
static VALUE nm_multiply(VALUE left_v, VALUE right_v) {
  NMATRIX *left, *right;

  // left has to be of type NMatrix.
  CheckNMatrixType(left_v);

  UnwrapNMatrix( left_v, left );

  if (RUBYVAL_IS_NUMERIC(right_v))
    return matrix_multiply_scalar(left, right_v);

  else if (TYPE(right_v) == T_ARRAY)
    rb_raise(rb_eNotImpError, "for matrix-vector multiplication, please use an NVector instead of an Array for now");

  //if (RDATA(right_v)->dfree != (RUBY_DATA_FUNC)nm_delete) {
  else if (TYPE(right_v) == T_DATA && RDATA(right_v)->dfree == (RUBY_DATA_FUNC)nm_delete) { // both are matrices
    UnwrapNMatrix( right_v, right );

    if (left->storage->shape[1] != right->storage->shape[0])
      rb_raise(rb_eArgError, "incompatible dimensions");

    if (left->stype != right->stype)
      rb_raise(rb_eNotImpError, "matrices must have same stype");

    return matrix_multiply(left, right);

  } else rb_raise(rb_eTypeError, "expected right operand to be NMatrix, NVector, or single numeric value");

  return Qnil;
}

/*
 * Get the rank of an NMatrix (the number of dimensions).
 *
 * In other words, if you set your matrix to be 3x4, the rank is 2. If the
 * matrix was initialized as 3x4x3, the rank is 3.
 *
 * This function may lie slightly for NVectors, which are internally stored as
 * rank 2 (and have an orientation), but act as if they're rank 1.
 */
static VALUE nm_rank(VALUE self) {
  return INT2FIX(NM_STORAGE(self)->rank);
}

/*
 * Get the shape (dimensions) of a matrix.
 */
static VALUE nm_shape(VALUE self) {
  STORAGE* s   = NM_STORAGE(self);
  size_t index;

  // Copy elements into a VALUE array and then use those to create a Ruby array with rb_ary_new4.
  VALUE* shape = ALLOCA_N(VALUE, s->rank);
  for (index = 0; index < s->rank; ++index)
    shape[index] = INT2FIX(s->shape[index]);

  return rb_ary_new4(s->rank, shape);
}

/*
 * Get the storage type (stype) of a matrix, e.g., :yale, :dense, or :list.
 */
static VALUE nm_stype(VALUE self) {
  ID stype = rb_intern(STYPE_NAMES[NM_STYPE(self)]);
  return ID2SYM(stype);
}

/*
 * Is this matrix symmetric?
 */
static VALUE nm_symmetric(VALUE self) {
  return is_symmetric(self, false);
}

/*
 * Get a slice of an NMatrix.
 */
static VALUE nm_xslice(int argc, VALUE* argv, void* (*slice_func)(STORAGE*, SLICE*), void (*delete_func)(NMATRIX*), VALUE self) {
  VALUE result = Qnil;

  if (NM_RANK(self) == (size_t)(argc)) {
    SLICE* slice = get_slice((size_t)(argc), argv, self);

    // TODO: Slice for List, Yale types

    if (slice->single) {

      static void* (*ttable[NUM_STYPES])(STORAGE*, SLICE*) = {
        dense_storage_ref,
        list_storage_ref,
        yale_storage_ref
      };

      fprintf(stderr, "single: ");
      for (size_t i = 0; i < NM_RANK(self); ++i) {
        fprintf(stderr, "%u(%u) ", slice->coords[i], slice->lengths[i]);
      }
      fprintf(stderr, "\n");

      DENSE_STORAGE* s = NM_DENSE_STORAGE(self);

      if (NM_DTYPE(self) == RUBYOBJ)  result = *reinterpret_cast<VALUE*>( ttable[NM_STYPE(self)](NM_STORAGE(self), slice) );
      else                            result = rubyobj_from_cval( ttable[NM_STYPE(self)](NM_STORAGE(self), slice), NM_DTYPE(self) ).rval;

    } else {

      if (NM_STYPE(self) == DENSE_STORE) {
        STYPE_MARK_TABLE(mark_table);

        NMATRIX* mat = ALLOC(NMATRIX);
        mat->stype = NM_STYPE(self);
        mat->storage = (STORAGE*)((*slice_func)( NM_STORAGE(self), slice ));
        result = Data_Wrap_Struct(cNMatrix, mark_table[mat->stype], delete_func, mat);
      } else {
        rb_raise(rb_eNotImpError, "slicing only implemented for dense so far");
      }
    }

    free(slice);

  } else if (NM_RANK(self) < (size_t)(argc)) {
    rb_raise(rb_eArgError, "Coordinates given exceed matrix rank");
  } else {
    rb_raise(rb_eNotImpError, "This type slicing not supported yet");
  }

  return result;
}

//////////////////////
// Helper Functions //
//////////////////////

/*
 * Check to determine whether matrix is a reference to another matrix.
 */
bool is_ref(const NMATRIX* matrix) {
  // FIXME: Needs to work for other types
  if (matrix->stype != DENSE_STORE) {
    return false;
  }
  
  return ((DENSE_STORAGE*)(matrix->storage))->src == matrix->storage;
}

/*
 * Helper function for nm_symmetric and nm_hermitian.
 */
static VALUE is_symmetric(VALUE self, bool hermitian) {
  NMATRIX* m;
  UnwrapNMatrix(self, m);

  if (m->storage->shape[0] == m->storage->shape[1] and m->storage->rank == 2) {
		if (NM_STYPE(self) == DENSE_STORE) {
      if (hermitian) {
        dense_storage_is_hermitian((DENSE_STORAGE*)(m->storage), m->storage->shape[0]);
        
      } else {
      	dense_storage_is_symmetric((DENSE_STORAGE*)(m->storage), m->storage->shape[0]);
      }
      
    } else {
      // TODO: Implement, at the very least, yale_is_symmetric. Model it after yale/transp.template.c.
      rb_raise(rb_eNotImpError, "symmetric? and hermitian? only implemented for dense currently");
    }

  }

  return Qfalse;
}


///////////////////////
// Utility Functions //
///////////////////////

/*
 * Converts a string to a data type.
 */
dtype_t dtype_from_rbstring(VALUE str) {
  for (size_t index = 0; index < NUM_DTYPES; ++index) {
  	if (!strncmp(RSTRING_PTR(str), DTYPE_NAMES[index], RSTRING_LEN(str))) {
  		return static_cast<dtype_t>(index);
  	}
  }

  rb_raise(rb_eArgError, "Invalid data type string specified.");
}

/*
 * Converts a symbol to a data type.
 */
static dtype_t dtype_from_rbsymbol(VALUE sym) {
  size_t index;
  
  for (index = 0; index < NUM_DTYPES; ++index) {
    if (SYM2ID(sym) == rb_intern(DTYPE_NAMES[index])) {
    	return static_cast<dtype_t>(index);
    }
  }
  
  rb_raise(rb_eArgError, "Invalid data type symbol specified.");
}

/*
 * Guess the data type given a value.
 *
 * TODO: Probably needs some work for Bignum.
 */
static dtype_t dtype_guess(VALUE v) {
  switch(TYPE(v)) {
  case T_TRUE:
  case T_FALSE:
    return BYTE;
    
  case T_STRING:
    if (RSTRING_LEN(v) == 1) {
    	return BYTE;
    	
    } else {
    	rb_raise(rb_eArgError, "Strings of length > 1 may not be stored in a matrix.");
    }

#if SIZEOF_INT == 8
  case T_FIXNUM:
    return INT64;
    
  case T_RATIONAL:
    return RATIONAL128;
    
#else
# if SIZEOF_INT == 4
  case T_FIXNUM:
    return INT32;
    
  case T_RATIONAL:
    return RATIONAL64;
    
#else
  case T_FIXNUM:
    return INT16;
    
  case T_RATIONAL:
    return RATIONAL32;
# endif
#endif

  case T_BIGNUM:
    return INT64;

#if SIZEOF_FLOAT == 4
  case T_COMPLEX:
    return COMPLEX128;
    
  case T_FLOAT:
    return FLOAT64;
    
#else
# if SIZEOF_FLOAT == 2
  case T_COMPLEX:
    return COMPLEX64;
    
  case T_FLOAT:
    return FLOAT32;
# endif
#endif

  case T_ARRAY:
  	/*
  	 * May be passed for dense -- for now, just look at the first element.
  	 * 
  	 * TODO: Look at entire array for most specific type.
  	 */
  	
    return dtype_guess(RARRAY_PTR(v)[0]);

  case T_NIL:
  default:
    rb_raise(rb_eArgError, "Unable to guess a data type from provided parameters; data type must be specified manually.");
  }
}

/*
 * Documentation goes here.
 */
static SLICE* get_slice(size_t rank, VALUE* c, VALUE self) {
  size_t r;
  VALUE beg, end;
  int i;

  SLICE* slice = ALLOC(SLICE);
  slice->coords = ALLOC_N(size_t,rank);
  slice->lengths = ALLOC_N(size_t, rank);
  slice->single = true;

  for (r = 0; r < rank; ++r) {

    if (FIXNUM_P(c[r])) { // this used CLASS_OF before, which is inefficient for fixnum

      slice->coords[r]  = FIX2UINT(c[r]);
      slice->lengths[r] = 1;

    } else if (CLASS_OF(c[r]) == rb_cRange) {
        rb_range_values(c[r], &beg, &end, &i);
        slice->coords[r]  = FIX2UINT(beg);
        slice->lengths[r] = FIX2UINT(end) - slice->coords[r] + 1;
        slice->single     = false;

    } else {
      rb_raise(rb_eArgError, "cannot slice using class %s, needs a number or range or something", rb_obj_classname(c[r]));
    }

    if (slice->coords[r] + slice->lengths[r] > NM_SHAPE(self,r))
      rb_raise(rb_eArgError, "out of range");
  }

  return slice;
}

#ifdef BENCHMARK
/*
 * A simple function used when benchmarking NMatrix.
 */
static double get_time(void) {
  struct timeval t;
  struct timezone tzp;
  
  gettimeofday(&t, &tzp);
  
  return t.tv_sec + t.tv_usec*1e-6;
}
#endif

/*
 * The argv parameter will be either 1 or 2 elements.  If 1, could be either
 * initial or dtype.  If 2, is initial and dtype. This function returns the
 * dtype.
 */
static dtype_t interpret_dtype(int argc, VALUE* argv, stype_t stype) {
  int offset;
  
  switch (argc) {
  	case 1:
  		offset = 0;
  		break;
  	
  	case 2:
  		offset = 1;
  		break;
  		
  	default:
  		rb_raise(rb_eArgError, "Need an initial value or a dtype.");
  		break;
  }

  if (SYMBOL_P(argv[offset])) {
  	return dtype_from_rbsymbol(argv[offset]);
  	
  } else if (RUBYVAL_IS_STRING(argv[offset])) {
  	return dtype_from_rbstring(StringValue(argv[offset]));
  	
  } else if (stype == YALE_STORE) {
  	rb_raise(rb_eArgError, "Yale storage class requires a dtype.");
  	
  } else {
  	return dtype_guess(argv[0]);
  }
}

/*
 * Convert an Ruby value or an array of Ruby values into initial C values.
 */
static void* interpret_initial_value(VALUE arg, dtype_t dtype) {
  unsigned int index;
  void* init_val;
  
  if (RUBYVAL_IS_ARRAY(arg)) {
  	// Array
    
    init_val = ALLOC_N(int8_t, DTYPE_SIZES[dtype] * RARRAY_LEN(arg));
    for (index = 0; index < RARRAY_LEN(arg); ++index) {
    	rubyval_to_cval(RARRAY_PTR(arg)[index], dtype, (char*)init_val + (index * DTYPE_SIZES[dtype]));
    }
    
  } else {
  	// Single value
  	
    init_val = rubyobj_to_cval(arg, dtype);
  }

  return init_val;
}

/*
 * Convert the shape argument, which may be either a Ruby value or an array of
 * Ruby values, into C values.  The second argument is where the rank of the
 * matrix will be stored.  The function itself returns a pointer to the array
 * describing the shape, which must be freed manually.
 */
static size_t* interpret_shape(VALUE arg, size_t* rank) {
  size_t* shape;

  if (TYPE(arg) == T_ARRAY) {
    *rank = RARRAY_LEN(arg);
    shape = ALLOC_N(size_t, *rank);
    
    for (size_t index = 0; index < *rank; ++index) {
      shape[index] = FIX2UINT( RARRAY_PTR(arg)[index] );
    }
    
  } else if (FIXNUM_P(arg)) {
    *rank = 2;
    shape = ALLOC_N(size_t, *rank);
    
    shape[0] = FIX2UINT(arg);
    shape[1] = FIX2UINT(arg);
    
  } else {
    rb_raise(rb_eArgError, "Expected an array of numbers or a single Fixnum for matrix shape");
  }

  return shape;
}

/*
 * Convert a Ruby symbol or string into an storage type.
 */
static stype_t interpret_stype(VALUE arg) {
  if (SYMBOL_P(arg)) {
  	return stype_from_rbsymbol(arg);
  	
  } else if (RUBYVAL_IS_STRING(arg)) {
  	return stype_from_rbstring(StringValue(arg));
  	
  } else {
  	rb_raise(rb_eArgError, "Expected storage type");
  }
}

/*
 * Converts a symbol to an index type.
 */
static itype_t itype_from_rbsymbol(VALUE sym) {
  size_t index;

  for (index = 0; index < NUM_ITYPES; ++index) {
    if (SYM2ID(sym) == rb_intern(ITYPE_NAMES[index])) {
    	return static_cast<itype_t>(index);
    }
  }

  rb_raise(rb_eArgError, "Invalid index type specified.");
}

/*
 * Converts a string to a storage type. Only looks at the first three
 * characters.
 */
static stype_t stype_from_rbstring(VALUE str) {
  size_t index;
  
  for (index = 0; index < NUM_STYPES; ++index) {
    if (!strncmp(RSTRING_PTR(str), STYPE_NAMES[index], 3)) {
    	return static_cast<stype_t>(index);
    }
  }

  rb_raise(rb_eArgError, "Invalid storage type string specified");
  return DENSE_STORE;
}

/*
 * Converts a symbol to a storage type.
 */
static stype_t stype_from_rbsymbol(VALUE sym) {
  size_t index;
  
  for (index = 0; index < NUM_STYPES; ++index) {
    if (SYM2ID(sym) == rb_intern(STYPE_NAMES[index])) {
    	return static_cast<stype_t>(index);
    }
  }

  rb_raise(rb_eArgError, "Invalid storage type symbol specified");
  return DENSE_STORE;
}


//////////////////
// Math Helpers //
//////////////////


STORAGE* matrix_storage_cast_alloc(NMATRIX* matrix, dtype_t new_dtype) {
  if (matrix->storage->dtype == new_dtype && !is_ref(matrix))
    return matrix->storage;

  STYPE_CAST_COPY_TABLE(cast_copy_storage);
  return cast_copy_storage[matrix->stype][matrix->stype](matrix->storage, new_dtype);
}


STORAGE_PAIR binary_storage_cast_alloc(NMATRIX* left_matrix, NMATRIX* right_matrix) {
  STORAGE_PAIR casted;
  dtype_t new_dtype = Upcast[left_matrix->storage->dtype][right_matrix->storage->dtype];

  casted.left  = matrix_storage_cast_alloc(left_matrix, new_dtype);
  casted.right = matrix_storage_cast_alloc(right_matrix, new_dtype);

  return casted;
}


static VALUE matrix_multiply_scalar(NMATRIX* left, VALUE scalar) {
  rb_raise(rb_eNotImpError, "matrix-scalar multiplication not implemented yet");
  return Qnil;
}

static VALUE matrix_multiply(NMATRIX* left, NMATRIX* right) {
  ///TODO: multiplication for non-dense and/or non-decimal matrices

  // Make sure both of our matrices are of the correct type.
  STORAGE_PAIR casted = binary_storage_cast_alloc(left, right);

  size_t*  resulting_shape   = ALLOC_N(size_t, 2);
  resulting_shape[0] = left->storage->shape[0];
  resulting_shape[1] = right->storage->shape[1];

  // Sometimes we only need to use matrix-vector multiplication (e.g., GEMM versus GEMV). Find out.
  bool vector = false;
  if (resulting_shape[1] == 1) vector = true;

  static STORAGE* (*storage_matrix_multiply[NUM_STYPES])(const STORAGE_PAIR&, size_t*, bool) = {
    dense_storage_matrix_multiply,
    list_storage_matrix_multiply,
    yale_storage_matrix_multiply
  };

  STORAGE* resulting_storage = storage_matrix_multiply[left->stype](casted, resulting_shape, vector);
  NMATRIX* result = nm_create(left->stype, resulting_storage);

  // Free any casted-storage we created for the multiplication.
  // TODO: Can we make the Ruby GC take care of this stuff now that we're using it?
  // If we did that, we night not have to re-create these every time, right? Or wrong? Need to do
  // more research.
  static void (*free_storage[NUM_STYPES])(STORAGE*) = {
    dense_storage_delete,
    list_storage_delete,
    yale_storage_delete
  };

  if (left->storage != casted.left)   free_storage[result->stype](casted.left);
  if (right->storage != casted.right) free_storage[result->stype](casted.right);


  STYPE_MARK_TABLE(mark_table);

  if (result) return Data_Wrap_Struct(cNMatrix, mark_table[result->stype], nm_delete, result);
  return Qnil; // Only if we try to multiply list matrices should we return Qnil.
}

/////////////////////////////
// YALE-SPECIFIC FUNCTIONS //
/////////////////////////////
#ifdef DEBUG_YALE

/*
 * Get the size of a Yale matrix (the number of elements actually stored).
 *
 * For capacity (the maximum number of elements that can be stored without a resize), use capacity instead.
 */
static VALUE nm_yale_size(VALUE self) {
  if (NM_STYPE(self) != YALE_STORE) rb_raise(nm_eStorageTypeError, "wrong storage type");

  YALE_STORAGE* s = (YALE_STORAGE*)NM_STORAGE(self);

  return rubyobj_from_cval_by_itype((char*)(s->ija) + ITYPE_SIZES[s->itype]*(s->shape[0]), s->itype).rval;
}


/*
 * Get the A array of a Yale matrix (which stores the diagonal and the LU portions of the matrix).
 */
static VALUE nm_yale_a(VALUE self) {
  if (NM_STYPE(self) != YALE_STORE) rb_raise(nm_eStorageTypeError, "wrong storage type");

  YALE_STORAGE* s = NM_YALE_STORAGE(self);

  size_t size = yale_storage_get_size(s);
  VALUE* vals = ALLOCA_N(VALUE, size);

  for (size_t i = 0; i < size; ++i) {
    vals[i] = rubyobj_from_cval((char*)(s->a) + DTYPE_SIZES[s->dtype]*i, s->dtype).rval;
  }
  VALUE ary = rb_ary_new4(size, vals);

  for (size_t i = size; i < s->capacity; ++i)
    rb_ary_push(ary, Qnil);

  return ary;
}


/*
 * Get the diagonal ("D") portion of the A array of a Yale matrix.
 */
static VALUE nm_yale_d(VALUE self) {
  if (NM_STYPE(self) != YALE_STORE) rb_raise(nm_eStorageTypeError, "wrong storage type");

  YALE_STORAGE* s = (YALE_STORAGE*)NM_STORAGE(self);

  VALUE* vals = ALLOCA_N(VALUE, s->shape[0]);

  for (size_t i = 0; i < s->shape[0]; ++i) {
    vals[i] = rubyobj_from_cval((char*)(s->a) + DTYPE_SIZES[s->dtype]*i, s->dtype).rval;
  }
  return rb_ary_new4(s->shape[0], vals);
}


/*
 * Get the non-diagonal ("LU") portion of the A array of a Yale matrix.
 */
static VALUE nm_yale_lu(VALUE self) {
  if (NM_STYPE(self) != YALE_STORE) rb_raise(nm_eStorageTypeError, "wrong storage type");

  YALE_STORAGE* s = (YALE_STORAGE*)NM_STORAGE(self);

  size_t size = yale_storage_get_size(s);

  VALUE* vals = ALLOCA_N(VALUE, s->capacity - s->shape[0]);

  for (size_t i = 0; i < size - s->shape[0] - 1; ++i) {
    vals[i] = rubyobj_from_cval((char*)(s->a) + DTYPE_SIZES[s->dtype]*(s->shape[0] + 1 + i), s->dtype).rval;
  }

  VALUE ary = rb_ary_new4(size - s->shape[0] - 1, vals);

  for (size_t i = size; i < s->capacity; ++i)
    rb_ary_push(ary, Qnil);

  return ary;
}


/*
 * Get the IA portion of the IJA array of a Yale matrix. This gives the start and end positions of rows in the
 * JA and LU portions of the IJA and A arrays, respectively.
 */
static VALUE nm_yale_ia(VALUE self) {
  if (NM_STYPE(self) != YALE_STORE) rb_raise(nm_eStorageTypeError, "wrong storage type");

  YALE_STORAGE* s = (YALE_STORAGE*)NM_STORAGE(self);

  VALUE* vals = ALLOCA_N(VALUE, s->capacity - s->shape[0]);

  for (size_t i = 0; i < s->shape[0] + 1; ++i) {
    vals[i] = rubyobj_from_cval_by_itype((char*)(s->ija) + ITYPE_SIZES[s->itype]*i, s->itype).rval;
  }

  return rb_ary_new4(s->shape[0]+1, vals);
}


/*
 * Get the JA portion of the IJA array of a Yale matrix. This gives the column indices for entries in corresponding
 * positions in the LU portion of the A array.
 */
static VALUE nm_yale_ja(VALUE self) {
  if (NM_STYPE(self) != YALE_STORE) rb_raise(nm_eStorageTypeError, "wrong storage type");

  YALE_STORAGE* s = (YALE_STORAGE*)NM_STORAGE(self);

  size_t size = yale_storage_get_size(s);

  VALUE* vals = ALLOCA_N(VALUE, s->capacity - s->shape[0]);

  for (size_t i = 0; i < size - s->shape[0] - 1; ++i) {
    vals[i] = rubyobj_from_cval_by_itype((char*)(s->ija) + ITYPE_SIZES[s->itype]*(s->shape[0] + 1 + i), s->itype).rval;
  }

  VALUE ary = rb_ary_new4(size - s->shape[0] - 1, vals);

  for (size_t i = size; i < s->capacity; ++i)
    rb_ary_push(ary, Qnil);

  return ary;
}


/*
 * Get the IJA array of a Yale matrix.
 */
static VALUE nm_yale_ija(VALUE self) {
  if (NM_STYPE(self) != YALE_STORE) rb_raise(nm_eStorageTypeError, "wrong storage type");

  YALE_STORAGE* s = (YALE_STORAGE*)NM_STORAGE(self);

  size_t size = yale_storage_get_size(s);

  VALUE* vals = ALLOCA_N(VALUE, s->capacity - s->shape[0]);

  for (size_t i = 0; i < size; ++i) {
    vals[i] = rubyobj_from_cval_by_itype((char*)(s->ija) + ITYPE_SIZES[s->itype]*i, s->itype).rval;
  }

 VALUE ary = rb_ary_new4(size, vals);

  for (size_t i = size; i < s->capacity; ++i)
    rb_ary_push(ary, Qnil);

  return ary;
}
#endif // DEBUG_YALE

/////////////////
// Exposed API //
/////////////////

extern "C" {

/*
 * Create a dense matrix. Used by the NMatrix GSL fork. Unlike nm_create, this one copies all of the
 * arrays and such passed in -- so you don't have to allocate and pass a new shape object for every
 * matrix you want to create, for example. Same goes for elements.
 *
 * Returns a properly-wrapped Ruby object as a VALUE.
 *
 * TODO: Add a column-major option for libraries that use column-major matrices.
 */
VALUE rb_nmatrix_dense_create(dtype_t dtype, size_t* shape, size_t rank, void* elements, size_t length) {
  NMATRIX* nm;
  VALUE klass;
  size_t nm_rank;
  size_t* shape_copy;

  // Do not allow a rank of 1; if rank == 1, this should probably be an NVector instead, but that still has rank 2.
  if (rank == 1) {
    klass					= cNVector;
    nm_rank				= 2;
    shape_copy		= ALLOC_N(size_t, nm_rank);
    shape_copy[0]	= shape[0];
    shape_copy[1]	= 1;
    
  } else {
    klass				= cNMatrix;
    nm_rank			= rank;
    shape_copy	= ALLOC_N(size_t, nm_rank);
    memcpy(shape_copy, shape, sizeof(size_t)*nm_rank);
  }

  // Copy elements
  void* elements_copy = ALLOC_N(char, DTYPE_SIZES[dtype]*length);
  memcpy(elements_copy, elements, DTYPE_SIZES[dtype]*length);

  // allocate and create the matrix and its storage
  nm = nm_create(DENSE_STORE, dense_storage_create(dtype, shape_copy, rank, elements_copy, length));

  // tell Ruby about the matrix and its storage, particularly how to garbage collect it.
  return Data_Wrap_Struct(klass, dense_storage_mark, dense_storage_delete, nm);
}


/*
 * Create a dense vector. Used by the NMatrix GSL fork.
 *
 * Basically just a convenience wrapper for rb_nmatrix_dense_create().
 *
 * Returns a properly-wrapped Ruby NVector object as a VALUE.
 *
 * TODO: Add a transpose option for setting the orientation of the vector?
 */
VALUE rb_nvector_dense_create(dtype_t dtype, void* elements, size_t length) {
  size_t rank = 1, shape = length;
  return rb_nmatrix_dense_create(dtype, &shape, rank, elements, length);
}

}


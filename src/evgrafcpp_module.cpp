#include <Python.h>
#include <ndarraytypes.h>
#include <arrayobject.h>
#include <cstring>
#include <cassert>
#include <vector>
#include "crystalline.h"
#include "rectangular_lsap.h"


#ifdef __cplusplus
extern "C" {
#endif

static PyObject* error(PyObject* type, const char* msg)
{
	PyErr_SetString(type, msg);
	return NULL;
}

static PyObject* calculate_rmsd(PyObject* self, PyObject* args, PyObject* kwargs)
{
	(void)self;
	PyObject* obj_p = NULL;
	PyObject* obj_q = NULL;
	PyObject* obj_nbrcell = NULL;
	PyObject* obj_numbers = NULL;
	PyObject* obj_pcont = NULL;
	PyObject* obj_qcont = NULL;
	PyObject* obj_nbrcellcont = NULL;
	PyObject* obj_numberscont = NULL;
	if (!PyArg_ParseTuple(args, "OOOO", &obj_p, &obj_q, &obj_nbrcell, &obj_numbers))
		return NULL;

	// get numpy arrays in contiguous form
	obj_pcont = PyArray_ContiguousFromAny(obj_p, NPY_DOUBLE, 1, 2);
	if (obj_pcont == NULL)
		return error(PyExc_TypeError, "Invalid input data: P");

	obj_qcont = PyArray_ContiguousFromAny(obj_q, NPY_DOUBLE, 1, 2);
	if (obj_qcont == NULL)
		return error(PyExc_TypeError, "Invalid input data: Q");

	obj_nbrcellcont = PyArray_ContiguousFromAny(obj_nbrcell, NPY_DOUBLE, 1, 3);
	if (obj_nbrcellcont == NULL)
		return error(PyExc_TypeError, "Invalid input data: nbrcell");

	obj_numberscont = PyArray_ContiguousFromAny(obj_numbers, NPY_INT, 1, 1);
	if (obj_numberscont == NULL)
		return error(PyExc_TypeError, "Invalid input data: numbers");

	// validate numpy arrays
	if (PyArray_NDIM(obj_pcont) != 2)
		return error(PyExc_TypeError, "P must have shape N x 3");

	if (PyArray_NDIM(obj_qcont) != 2)
		return error(PyExc_TypeError, "Q must have shape N x 3");

	if (PyArray_DIM(obj_pcont, 1) != 3)
		return error(PyExc_TypeError, "P must contain three-dimensional coordinates");

	if (PyArray_DIM(obj_qcont, 1) != 3)
		return error(PyExc_TypeError, "Q must contain three-dimensional coordinates");

	if (PyArray_DIM(obj_pcont, 0) != PyArray_DIM(obj_qcont, 0))
		return error(PyExc_TypeError, "P and Q must contain same number of entries");

	if (PyArray_NDIM(obj_nbrcellcont) != 2
		|| PyArray_DIM(obj_nbrcellcont, 1) != 3)
		return error(PyExc_TypeError, "nbrcell must have shape m x 3 x 3");

	if (PyArray_NDIM(obj_numberscont) != 1)
		return error(PyExc_TypeError, "numbers array must be 1-dimensional");

	if (PyArray_DIM(obj_numberscont, 0) != PyArray_DIM(obj_pcont, 0))
		return error(PyExc_TypeError, "numbers array must contain N entries");

	int num_atoms = PyArray_DIM(obj_pcont, 0);
	int num_cells = PyArray_DIM(obj_nbrcellcont, 0);
	double* P = (double*)PyArray_DATA((PyArrayObject*)obj_p);
	double* Q = (double*)PyArray_DATA((PyArrayObject*)obj_q);
	double* nbrcells = (double*)PyArray_DATA((PyArrayObject*)obj_nbrcell);
	int* numbers = (int*)PyArray_DATA((PyArrayObject*)obj_numbers);

	npy_intp dim[1] = {num_atoms};
	PyObject* obj_permutation = PyArray_SimpleNew(1, dim, NPY_INT);
	int* permutation = (int*)PyArray_DATA((PyArrayObject*)obj_permutation);

	double cost = INFINITY;
	int res = crystalline_bipartite_matching(num_atoms, num_cells, P, Q, nbrcells, numbers, &cost, permutation);
	if (res != 0)
		return error(PyExc_RuntimeError, "bipartite matching failed");

	PyObject* result = Py_BuildValue("dO", cost, obj_permutation);
	Py_DECREF(obj_pcont);
	Py_DECREF(obj_qcont);
	Py_DECREF(obj_nbrcellcont);
	Py_DECREF(obj_numberscont);
	Py_DECREF(obj_permutation);
	return result;
}

static PyObject* linear_sum_assignment(PyObject* self, PyObject* args, PyObject* kwargs)
{
	(void)self;
	PyObject* obj_cost = NULL;
	PyObject* obj_costcont = NULL;
	if (!PyArg_ParseTuple(args, "O", &obj_cost))
		return NULL;

	// get numpy arrays in contiguous form
	obj_costcont = PyArray_ContiguousFromAny(obj_cost, NPY_DOUBLE, 1, 2);
	if (obj_costcont == NULL)
		return error(PyExc_TypeError, "Invalid input data: cost");

	// validate numpy arrays
	if (PyArray_NDIM(obj_costcont) != 2)
		return error(PyExc_TypeError, "cost matrix must be two-dimensional");

	int m = PyArray_DIM(obj_costcont, 0);
	int n = PyArray_DIM(obj_costcont, 1);
	int p = std::min(m, n);
	double* cost = (double*)PyArray_DATA((PyArrayObject*)obj_cost);

	npy_intp dim[1] = {p};
	PyObject* obj_permutation = PyArray_SimpleNew(1, dim, NPY_INT64);
	int64_t* permutation = (int64_t*)PyArray_DATA((PyArrayObject*)obj_permutation);

	PyObject* obj_range = PyArray_SimpleNew(1, dim, NPY_INT64);
	int64_t* range = (int64_t*)PyArray_DATA((PyArrayObject*)obj_range);
	for (int i=0;i<p;i++) {
		range[i] = i;
	}

	int res = solve_rectangular_linear_sum_assignment(m, n, cost, permutation);
	if (res != 0)
		return error(PyExc_RuntimeError, "linear_sum_assignment failed");

	PyObject* result = Py_BuildValue("OO", obj_range, obj_permutation);
	Py_DECREF(obj_costcont);
	Py_DECREF(obj_range);
	Py_DECREF(obj_permutation);
	return result;
}

static PyMethodDef evgrafcpp_methods[] = {
	{
		"calculate_rmsd",
		(PyCFunction)calculate_rmsd,
		METH_VARARGS,
		"Calculates the RMSD between two crystal structures."
	},
	{
		"linear_sum_assignment",
		(PyCFunction)linear_sum_assignment,
		METH_VARARGS,
		"Solve the linear sum assignment problem."
	},
	{NULL}
};

static struct PyModuleDef evgrafcpp_definition = {
	PyModuleDef_HEAD_INIT,
	"evgrafcpp",
	"evgraf C++ module.",
	-1,
	evgrafcpp_methods,
	NULL,
	NULL,
	NULL,
	NULL,
};

PyMODINIT_FUNC PyInit_evgrafcpp(void)
{
	Py_Initialize();
	import_array();
	return PyModule_Create(&evgrafcpp_definition);
}

#ifdef __cplusplus
}
#endif


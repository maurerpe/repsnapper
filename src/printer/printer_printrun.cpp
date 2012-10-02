/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2012 martin.dieringer@gmx.de

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include "printer_printrun.h"

#define FROMFILE 0
#if FROMFILE
#else
#include "printcore_py.h"
#endif

// passing const char to Python ...
#define CHARS(var,str) \
  char var[sizeof(str)];	  \
  strncpy(var, str, sizeof(str)); \
  var[sizeof(var) - 1] = '\0'



Printer::Printer(View *view)
 :   printing (false),
     lastdonelines(0),
     lasttimeshown(0),
     debug_output(false),
     m_model(NULL),
     inhibit_print (false)
{
  gcode_iter = NULL;
  m_view = view;

  listen_thread = 0;

  Py_Initialize();
  PyEval_InitThreads();
  //PyEval_AcquireLock();
  pyThreadState = NULL;
  // save a pointer to the main PyThreadState object
  //pyThreadState = PyThreadState_Get();
  // release the lock
  //PyEval_ReleaseLock();
  //Py_BEGIN_ALLOW_THREADS;
#if FROMFILE
  cerr << "running file printcore.py ..." << endl;
  PyRun_SimpleString("import sys\nsys.path.append('.')\n");
  pyName = PyString_FromString("printcore");
  pyModule = PyImport_Import(pyName);
#else
  //cerr << printcore_py << endl;
  if (PyRun_SimpleString(printcore_py) != 0)  {
    cerr << "Could not run printcore python code" << endl;
    return;
  }
  pyModule = PyImport_AddModule("__main__");
#endif

  if (!pyModule)
    cerr << "error loading module" << endl;

  PyRun_SimpleString("print 'Python Version',sys.version\n");

  //cerr << "mo " << pyModule << endl;
  pyDict = PyModule_GetDict(pyModule);

  // Create an instance
  CHARS (cl, "printcore");
  PyObject *pyClass = PyDict_GetItemString(pyDict, cl);
  if (PyCallable_Check(pyClass)) {
    pyPrintCore = PyObject_CallObject(pyClass, NULL);
  }
  if (!pyPrintCore)
    cerr << "error loading core module" << endl;
  //Py_END_ALLOW_THREADS;
}

Printer::~Printer()
{
  Stop();
  cerr << "disconnecting ... ";
  serial_try_connect(false);
  cerr << "ok" << endl;
  if (Py_IsInitialized())
    Py_Finalize();
  Py_Finalize();
}


void Printer::listen_thread_run()
{

  while(run_listen) {
    cerr <<"call _listen_single" << endl;

    if (pyPrintCore) {
      //Glib::Mutex::Lock lock (print_mutex);
      //PyThreadState *_save = PyEval_SaveThread();
      //cerr << "save " << _save << endl;
      PyGILState_STATE gstate = PyGILState_Ensure();
#if 1
      PyObject *printer = PyObject_GetAttrString(pyPrintCore, "printer");
      if(!printer) {
	PyErr_Print();
      } else {
	CHARS (readline, "readline");
	cerr <<"now" << endl;
	PyObject *pLine = PyObject_CallMethod(printer, readline, NULL);
	cerr <<"end" << endl;

	if(!pLine) {
	  PyErr_Print();
	}
	else {
	  string line = string(PyString_AsString(pLine));
	  cerr << "line: " << line << endl;
	  parse_response(line);
	}
      }
      //Py_END_ALLOW_THREADS;
#else
      CHARS (meth, "_listen_single");
      //CHARS (meth, "_listen");
      cerr << "_listen" << endl;
      PyObject *pValue =
	PyObject_CallMethod(pyPrintCore, meth, NULL);
      if(!pValue) {
	PyErr_Print();
	return;
      }

#endif
      PyGILState_Release(gstate);
      // PyEval_RestoreThread(_save);
    }
    cerr <<"end _listen_single" << endl;
    Glib::usleep(100000);
  }
  return;
}

bool Printer::do_connect (bool connect)
{
  if (connect) {

    if (pyPrintCore) {
      //Glib::Mutex::Lock lock (print_mutex);
      PyGILState_STATE gstate = PyGILState_Ensure();

      //Py_BEGIN_ALLOW_THREADS;
      PyObject *pValue;
      const char * port = m_model->settings.Hardware.PortName.c_str();
      cerr << "connecting to "<< port << " ... ";
      CHARS (meth, "connect");
      CHARS (format, "(si)");
      const int speed = m_model->settings.Hardware.SerialSpeed;
      //PyGILState_STATE gstate;
      // gstate = PyGILState_Ensure();
      pValue = PyObject_CallMethod(pyPrintCore, meth, format,
				   port, speed);
      // PyGILState_Release(gstate);
      if(!pValue) {
	PyErr_Print();
	return false;
      }
      cerr << "ok" << endl;
      //Py_END_ALLOW_THREADS;
      PyGILState_Release(gstate);
    }

    if (IsConnected()) {
      try {
	run_listen = true;
	listen_thread =
	  Glib::Thread::create(sigc::mem_fun(*this, &Printer::listen_thread_run),
			       true); // joinable (ignored)
      } catch (Glib::ThreadError e) {
	cerr << "can't start print thread: "<< e.what() << endl;
	//return false;
      }

    }
    return true;

  } else { // disconnect
      run_listen = false;
      if (listen_thread != 0) {
	if (listen_thread->joinable())
	  listen_thread->join();
	listen_thread = 0;
      }
      bool result = true;
      PyGILState_STATE gstate = PyGILState_Ensure();
      if (pyPrintCore) {
	CHARS (meth,  "disconnect");
	PyObject *pValue =
	  PyObject_CallMethod(pyPrintCore, meth, NULL);
	if(!pValue) {
	  PyErr_Print();
	  result = false;
	}
      }
      PyGILState_Release(gstate);
      return result;
  }
}


void Printer::log(string s, RR_logtype type)
{
  switch (type) {
  case LOG_ERROR: error_buffer  +=s; break;
  case LOG_ECHO:  echo_buffer   +=s; break;
  default:
  case LOG_COMM:  commlog_buffer+=s; break;
  }
}

// void Printer::update_core_settings ()
// {
//   if (m_model) {
// #if IOCHANNEL
//     debug_output = m_model->settings.Display.CommsDebug;
// #else
//     if (device==NULL) return;
//     rr_dev_enable_debugging(device, m_model->settings.Display.CommsDebug);
// #endif

//   }
// }

void Printer::Pause()
{
  if (!IsConnected())
    return;
  if (pyPrintCore) {
    Glib::Mutex::Lock lock (print_mutex);
    CHARS (meth, "pause");
    PyObject *pValue =
      PyObject_CallMethod(pyPrintCore, meth, NULL);
    if(!pValue) {
      PyErr_Print();
      return;
    }
  }
  set_printing (false);
}

void Printer::Continue()
{
  if (!IsConnected())
    return;
  if (pyPrintCore) {
    Glib::Mutex::Lock lock (print_mutex);
    //Py_BEGIN_ALLOW_THREADS;
    CHARS (meth, "resume");
    PyObject *pValue =
      PyObject_CallMethod(pyPrintCore, meth, NULL);
    if(!pValue) {
      PyErr_Print();
      return;
    }
    //Py_END_ALLOW_THREADS;
  }
  set_printing (true);
}

PyObject * Printer::p_get_attr(const char * name) const
{
  if (!pyPrintCore) return NULL;
  PyObject *pObj = PyObject_GetAttrString(pyPrintCore, name);
  PyObject *pValue = NULL;
  if(pObj) {
    return pObj;
    // pValue = PyEval_CallObject(pObj,NULL);
    // //PyObject_GetAttrString(pyPrintCore, meth);
    // if(!pValue) {
    //   cerr <<" value error: " ;
    //   PyErr_Print();
    // }
  } else {
    cerr <<" object error: " ;
    PyErr_Print();
  }
  return pValue;
}

bool Printer::IsConnected()
{
  bool ret = false;
  if (pyPrintCore) {
    //Glib::Mutex::Lock lock (print_mutex);
    PyGILState_STATE gstate = PyGILState_Ensure();

    //Py_BEGIN_ALLOW_THREADS;
    CHARS (meth, "is_online");
    PyObject *pValue =
      PyObject_CallMethod(pyPrintCore, meth, NULL);
    //PyObject *pValue = p_get_attr("online");
    if(!pValue) return false;
    long blong = PyInt_AsLong(pValue);
    ret = (blong == 1);
    //Py_END_ALLOW_THREADS;
    PyGILState_Release(gstate);
  }
  // cerr << "online: "<< ret << endl;
  if (ret)
    signal_serial_state_changed.emit (SERIAL_CONNECTED);
  else
    signal_serial_state_changed.emit (SERIAL_DISCONNECTED);
  return ret;
}

void Printer::Reset()
{
  // if (!IsConnected())
  //   return;
  cerr << "reset?"<< endl;
  if (pyPrintCore) {
    //Py_BEGIN_ALLOW_THREADS;
    PyGILState_STATE gstate = PyGILState_Ensure();
    //Glib::Mutex::Lock lock (print_mutex);
    CHARS (meth, "reset");
    PyObject *pValue =
      PyObject_CallMethod(pyPrintCore, meth, NULL);
    if(!pValue) {
      PyErr_Print();
    }
    PyGILState_Release(gstate);
    //Py_END_ALLOW_THREADS;
  }
}



void Printer::Print()
{
  if (!IsConnected()) {
    alert (_("Not connected to printer.\nCannot start printing"));
    return;
  }

  delete (gcode_iter);
  gcode_iter = m_model->gcode.get_iter();

  cerr << "sending text" << endl;
  Glib::ustring text = gcode_iter->all_text();


  if (pyPrintCore) {
    PyObject *pValue = NULL;
    PyGILState_STATE gstate = PyGILState_Ensure();
    CHARS (meth, "startprint_text");
    CHARS (format, "s");
    pValue = PyObject_CallMethod(pyPrintCore, meth, format, text.c_str());
    PyGILState_Release(gstate);
    if(!pValue) {
      PyErr_Print();
      return;
    }
  }
  set_printing (true);

  // uint num = 0;
  // while (true) {
  //   if (gcode_iter->finished()) break;
  //   string line = gcode_iter->next_line_stripped();
  //   //cerr << "line: " << line;
  //   Send(line);
  //   //if (num>100) break;
  //   num++;
  // }


  // if (pyPrintCore) {
  //   CHARS (meth, "startprint");
  //   PyObject *pValue =
  //     PyObject_CallMethod(pyPrintCore, meth, NULL);

  //   if(!pValue) {
  //     PyErr_Print();
  //     return;
  //   }
  // }

  // if (device==NULL) return;
  // if (!rr_dev_is_connected (device)) {
  //   alert (_("Not connected to printer.\nCannot start printing"));
  //   return;
  // }

  // assert(m_model != NULL);


  // rr_dev_reset (device);
  // rr_dev_set_paused (device, RR_PRIO_NORMAL, 0);
}


long Printer::get_next_line(string &line)
{
  return -1;
}

// add to queue
bool Printer::Send(string str)
{
  if (!IsConnected()) {
    error (_("Can't send command"), _("You must first connect to a device!"));
    return false;
  }
  std::transform(str.begin(), str.end(), str.begin(), ::toupper);

  if (pyPrintCore) {
    //Glib::Mutex::Lock lock (print_mutex);
    //Py_BEGIN_ALLOW_THREADS;
    PyObject *pValue = NULL;
    PyGILState_STATE gstate = PyGILState_Ensure();
    CHARS (meth, "send");
    CHARS (format, "s");
    pValue = PyObject_CallMethod(pyPrintCore, meth, format, str.c_str());
    PyGILState_Release(gstate);
    if(!pValue) {
      PyErr_Print();
      return false;
    }
    //Py_END_ALLOW_THREADS;
  }
  return true;
}

// send immediately
bool Printer::SendNow(string str)
{
  if (!IsConnected()) {
    error (_("Can't send command"), _("You must first connect to a device!"));
    return false;
  }
  std::transform(str.begin(), str.end(), str.begin(), ::toupper);
  //cerr << "sendnow "<<str  << endl;
  if (pyPrintCore) {
    //Glib::Mutex::Lock lock (print_mutex);
    //Py_BEGIN_ALLOW_THREADS;
    PyObject *pValue = NULL;
    PyGILState_STATE gstate = PyGILState_Ensure();
    CHARS (meth, "send_now");
    CHARS (format, "s");
    pValue = PyObject_CallMethod(pyPrintCore, meth, format, str.c_str());
    PyGILState_Release(gstate);
    if(!pValue) {
      PyErr_Print();
      return false;
    }
    //Py_END_ALLOW_THREADS;
  }
  //cerr << "sendnow done " << endl;
  return true;
}

void Printer::Stop()
{
  Pause();
}


unsigned long Printer::set_resend(unsigned long print_lineno)
{
  cerr << "resend not implemented! " << print_lineno  << endl;
  return print_lineno;
}

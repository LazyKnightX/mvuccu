#include <v8pp/module.hpp>
#include <v8.h>

#include <QtCore/qfileinfo>

#include "LogManager.h"

namespace console {
	void log(v8::FunctionCallbackInfo<v8::Value> const& args) {
		v8::HandleScope handle_scope(args.GetIsolate());
		for (int i = 0; i < args.Length(); i++) {
			if (i > 0) LogManager::instance().log(" ");
			v8::String::Utf8Value str(args[i]);
			LogManager::instance().log(*str);
		}
		LogManager::instance().log("\n");
	}

	void err(v8::FunctionCallbackInfo<v8::Value> const& args) {
		v8::HandleScope handle_scope(args.GetIsolate());
		for (int i = 0; i < args.Length(); i++) {
			if (i > 0) LogManager::instance().err(" ");
			v8::String::Utf8Value str(args[i]);
			LogManager::instance().err(*str);
		}
		LogManager::instance().err("\n");
	}

	v8::Handle<v8::Value> init(v8::Isolate *iso) {
		v8pp::module m(iso);
		m.set("log", &log);
		m.set("err", &err);
		return m.new_instance();
	}
};

#define V8Func(name) v8::Handle<v8::Value> name(v8::FunctionCallbackInfo<v8::Value> const& args)

#include <QtCore/qmap.h>
#include <QtCore/qstack.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile>
#include <v8pp/call_v8.hpp>

namespace require {
	v8::Handle<v8::Value> require(const char * path);
	const QString warpper[] = {
		"(function (exports, require, module, __filename, __dirname) { ", "\n});" 
	};
	
	QMap<QString, v8::Persistent<v8::Value>> _internal;
	QMap<QDir, v8::Persistent<v8::Value>> _external;

	struct package_state {
		QDir _current_dir;
		QMap<QString, v8::Persistent<v8::Value>> _current_module;
	};

	QStack<package_state> package_stack;

	void addInternalModule(QString mod, v8::Handle<v8::Value> val) {
		v8::Persistent<v8::Value> p(v8::Isolate::GetCurrent(), val);
		_internal[mod] = p;
	}
	

	/* run .js file
	   change _current_dir to that file
	   hold _current_module  */
	v8::Handle<v8::Value> run(const char * path) {
		auto isolate = v8::Isolate::GetCurrent();
		v8::EscapableHandleScope handle_scope(isolate);
		QFileInfo f(path);
		if (f.exists()) {
			QDir _old_dir = package_stack.top()._current_dir;
			package_stack.top()._current_dir = f.absoluteDir();
			QFile _f(path);
			if (_f.open(QIODevice::ReadOnly)) {
				QString s = _f.readAll();
				s = warpper[0] + s + warpper[1];
				v8::Local<v8::Object> module = v8::Object::New(isolate);
				v8::Local<v8::Object> exports = v8::Object::New(isolate);
				module->Set(v8::String::NewFromUtf8(isolate, "exports"), exports);
				//v8::Persistent<v8::Object> module(isolate, );

				auto filename = v8::String::NewFromUtf8(isolate, f.fileName().toUtf8().data);
				auto dirname = v8::String::NewFromUtf8(isolate, f.absoluteDir().absolutePath().toUtf8().data);

				
				v8::Local<v8::Context> context = v8::Context::New(isolate);
				v8::Context::Scope context_scope(context);
				v8::Local<v8::String> source =
					v8::String::NewFromUtf8(isolate, s.toUtf8().data,
					v8::NewStringType::kNormal).ToLocalChecked();
				v8::Local<v8::Script> script = v8::Script::Compile(context, source).ToLocalChecked();
				v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
				auto f = v8::Local<v8::Function>::Cast(result);
				v8pp::call_v8(isolate, f, isolate->GetCurrentContext()->Global(), exports, &require, module, )
			}

		} else {
			isolate->ThrowException(v8::String::NewFromUtf8(isolate, QString("File (%1) not found.").arg(f.absoluteFilePath()).toUtf8().data));
			return handle_scope.Escape(v8::Null(isolate));
		}
	}

	/* run package
	   push package_stack  */
	v8::Handle<v8::Value> load(const char * name) {
		auto &s = package_stack.top();
		QDir path(s._current_dir);
		if (path.cd("node_modules"))
			if (path.cd(name)) {
				if (QFile(path.absoluteFilePath("package.json")).exists()) {
					package_state S;
					S._current_dir = path;
					
				}
				
			}
		return v8::Null(v8::Isolate::GetCurrent());
	}

	// require expose to js
	v8::Handle<v8::Value> require(const char * path) {
		QString p(path);
		// find in internal
		if (_internal.find(p) != _internal.end()) {
			return v8::Local<v8::Value>::New(v8::Isolate::GetCurrent(), _internal[p]);
		}

		if (p.startsWith("./")) {
			// lookup file

		} else {
			auto _modules = package_stack.top()._current_module;
			if (_modules.find(path) != _modules.end()) {
				return v8::Local<v8::Value>::New(v8::Isolate::GetCurrent(), _modules[path]);
			} else {
				load(path);
			}
		}
		

	}
};


namespace fs {

	QString workingPath() {
		return require::package_stack.top()._current_dir.absolutePath();
	}

	const int F_OK = 1;
	const int R_OK = 2;
	const int W_OK = 4;
	const int X_OK = 8;
	v8::Handle<v8::Value> accessSync(v8::FunctionCallbackInfo<v8::Value> const& args) {
		QFileInfo f(*v8::String::Utf8Value(args[0]));
		int p = 0;
		p |= (f.exists()) * F_OK;
		p |= (f.isReadable()) * R_OK;
		p |= (f.isWritable()) * W_OK;
		p |= (f.isExecutable()) * X_OK;
		int mode = 1;
		v8::Isolate* isolate = v8::Isolate::GetCurrent();
		if (args.Length() == 2) {
			mode = v8pp::from_v8<int>(isolate, args[1]);
		}
		if ((mode != 0) && ((mode & p) != 0))
			return v8::Null(isolate);
		isolate->ThrowException(v8::String::NewFromUtf8(isolate, "no access"));
	}


	v8::Handle<v8::Value> readFileSync(v8::FunctionCallbackInfo<v8::Value> const& args) {
		v8::Isolate* isolate = v8::Isolate::GetCurrent();
		return v8::Null(isolate);
	}

	v8::Handle<v8::Value> init(v8::Isolate *iso) {
		v8pp::module m(iso);
		m.set("readFileSync", &readFileSync);
		m.set("accessSync", &accessSync);

		return m.new_instance();
	}
};
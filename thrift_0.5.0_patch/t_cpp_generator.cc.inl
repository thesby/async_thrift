/** @file
* @brief extension for thrift c++ generator
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
* Portable
*/
#include <string.h>
#include <algorithm>

string t_cpp_generator::async_if_function_signature(t_function * function,
                                                    const string& prefix,
                                                    bool name_params)
{
  t_type * ret_type = function->get_returntype();
  t_struct * arglist = function->get_arglist();

  string ret;
  ret.reserve(128);

  if (prefix.empty()) ret += "virtual ";
  ret += "void ";//return type is void
  if (!prefix.empty()) ret += prefix;

  ret += "async_";
  ret += function->get_name();

  ret += "(";

  //the first argument is the return value if it is not "void",
  //which is different from the function 'function_signature'
  if (!ret_type->is_void())
  {
    ret += type_name(ret_type);
    ret += "& _return, ";
  }

  //the following is the real argument list of the RPC function
  if (!arglist->get_members().empty())
  {
    ret += argument_list(arglist, name_params);
    ret += ", ";
  }

  //the last argument is the callback
  ret += "::apache::thrift::async::AsyncRPCCallback callback";

  ret += ")";
  return ret;
}


string t_cpp_generator::async_process_function_signature(t_function * function, const string& prefix)
{
  string ret;
  ret.reserve(128);

  ret += "void ";
  if (!prefix.empty()) ret += prefix;
  ret += "process_";
  ret += function->get_name();

  ret += "(";
  ret += "int32_t seqid, ";
  ret += "::apache::thrift::async::AsyncProcessorCallback callback, ";
  ret += "::apache::thrift::protocol::TProtocol * input_protocol, ";
  ret += "::apache::thrift::protocol::TProtocol * output_protocol";
  ret += ")";
  return ret;
}


string t_cpp_generator::async_complete_function_signature(t_function * function, const string& svcname, const string& prefix)
{
  string ret;
  ret.reserve(128);

  ret += "void ";
  if (!prefix.empty()) ret += prefix;
  ret += "complete_";
  ret += function->get_name();

  ret += "(";
  ret += "int32_t seqid, ";
  ret += "::apache::thrift::async::AsyncProcessorCallback callback, ";
  ret += "::apache::thrift::protocol::TProtocol * input_protocol, ";
  ret += "::apache::thrift::protocol::TProtocol * output_protocol, ";
  ret += "boost::shared_ptr<" + svcname + "_" + function->get_name() + "_args> " + function->get_name() + "_args, ";
  if (!function->is_oneway())
    ret += "boost::shared_ptr<" + svcname + "_" + function->get_name() + "_result> " + function->get_name() + "_result, ";
  ret += "const boost::system::error_code& ec";
  ret += ")";
  return ret;
}


void t_cpp_generator::generate_async(t_service* tservice)
{
  string svcname = tservice->get_name();
  string async_svcname = "Async" + svcname;
  t_service * base_tservice = tservice->get_extends();
  string async_base_svcname;
  if (base_tservice)
    async_base_svcname = "Async" + base_tservice->get_name();

  //open .h and .cpp
  string f_async_header_name = get_out_dir() + async_svcname + ".h";
  string f_async_source_name = get_out_dir() + async_svcname + ".cpp";
  f_async_header_.open(f_async_header_name.c_str());
  f_async_service_.open(f_async_source_name.c_str());

  //.h
  string macro_guard = async_svcname + "_H";
  std::transform(macro_guard.begin(), macro_guard.end(), macro_guard.begin(), ::toupper);

  //header comment
  f_async_header_ << autogen_comment();

  //macro guard
  f_async_header_ << "#ifndef " << macro_guard << endl <<
    "#define " << macro_guard << endl;
  f_async_header_ << endl;

  //include headers
  f_async_header_ << "#include <AsyncThriftClient.h>//add include path to CPPFLAGS(-Ixxx)" << endl;
  f_async_header_ << "#include <AsyncProcessor.h>//add include path to CPPFLAGS(-Ixxx)" << endl;
  f_async_header_ << "#include \"" << get_include_prefix(*get_program()) << svcname << ".h\"" << endl;
  if (base_tservice)
    f_async_header_ << "#include \"" << get_include_prefix(*get_program()) << async_base_svcname << ".h\"" << endl;
  f_async_header_ << endl;

  //namespace
  f_async_header_ << ns_open_ << endl;
  f_async_header_ << endl;

  //.cpp
  //header comment
  f_async_service_ << autogen_comment();

  //include headers
  f_async_service_ << "#include <boost/bind.hpp>" << endl;
  f_async_service_ << "#include \"" << get_include_prefix(*get_program()) << async_svcname << ".h\"" << endl << endl;
  f_async_service_ << ns_open_ << endl << endl;


  //generate some real stuffs
  generate_async_client(tservice);
  generate_async_if_and_processor(tservice);


  //.cpp
  f_async_service_ << ns_close_ << endl;

  //.h
  f_async_header_ << ns_close_ << endl << endl;
  f_async_header_ << "#endif //" << macro_guard << endl;

  //close .h and .cpp
  f_async_service_.close();
  f_async_header_.close();
}


void t_cpp_generator::generate_async_client(t_service* tservice)
{
  string svcname = tservice->get_name();
  t_service * base_tservice = tservice->get_extends();
  string async_base_svcname;
  if (base_tservice)
    async_base_svcname = "Async" + base_tservice->get_name();

  string if_class_name = svcname + "If";
  string client_class_name = svcname + "Client";
  string async_client_class_name = "Async" + svcname + "Client";
  string base_async_client_class_name;
  if (base_tservice)
    base_async_client_class_name = "Async" + base_tservice->get_name() + "Client";

  vector<t_function*> functions = tservice->get_functions();

  /************************************************************************/
  indent_up();
  //class AsyncClient header(.h)
  string client_base_class_name;
  f_async_header_ << "class " << async_client_class_name << endl;
  f_async_header_ << indent() << ": virtual public " << if_class_name << "," << endl;
  if (base_tservice)
  {
    f_async_header_ << indent() << "public " <<
      namespace_prefix(base_tservice->get_program()->get_namespace("cpp")) << base_async_client_class_name << endl;
    client_base_class_name = base_async_client_class_name;
  }
  else
  {
    f_async_header_ << indent() << "public ::apache::thrift::async::AsyncThriftClient" << endl;
    client_base_class_name = "AsyncThriftClient";
  }

  f_async_header_ << "{" << endl;
  f_async_header_ << "public:" << endl;

  //ctor and dtor(.h)
  f_async_header_ << indent() <<
    async_client_class_name << "();" << endl;
  f_async_header_ << indent() <<
    "explicit " << async_client_class_name << "(const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket);" << endl;
  f_async_header_ << indent() <<
    "virtual ~" << async_client_class_name << "();" << endl << endl;

  //RPC function(.h)
  //every RPC function is assigned to a enum value for internal usage
  vector<string> function_op_enums;

  for (size_t i=0; i<functions.size(); i++)
  {
    t_function * function = functions[i];

    //async RPC
    indent(f_async_header_) <<
      async_if_function_signature(function) << ";" << endl;
    function_op_enums.push_back("kasync_" + function->get_name());

    //sync RPC
    f_async_header_ <<
      indent() << "virtual " << function_signature(function) << ";" << endl;
  }
  f_async_header_ << endl;

  //enum function_op_enums(.h)
  f_async_header_ <<
    "public:" << endl <<
    indent() << "enum {" << endl;
  indent_up();

  if (base_tservice)
  {
    f_async_header_ <<
      indent() << "kasync_" << async_client_class_name << "_begin = "
      << namespace_prefix(base_tservice->get_program()->get_namespace("cpp")) << base_async_client_class_name <<
      "::kasync_"<< base_async_client_class_name << "_end," << endl;
  }
  else
  {
    f_async_header_ <<
      indent() << "kasync_" << async_client_class_name << "_begin = 0," << endl;
  }

  for (size_t i=0; i<function_op_enums.size(); i++)
  {
    f_async_header_ <<
      indent() << function_op_enums[i] << "," << endl;
  }
  f_async_header_ <<
    indent() << "kasync_" << async_client_class_name << "_end," << endl;
  indent_down();
  f_async_header_ <<
    indent() << "};" << endl << endl;

  //key virtual function: fill_result(.h)
  f_async_header_ <<
    "protected:" << endl <<
    indent() << "virtual void fill_result(AsyncOp& op);" << endl << endl;

  f_async_header_ <<
    "private:" << endl <<
    indent() << "boost::shared_ptr<" << client_class_name << "> client_;" << endl;

  //class AsyncClient end(.h)
  f_async_header_ <<
    "};" << endl << endl;
  indent_down();

  /************************************************************************/

  //ctor and dtor(.cpp)
  indent_up();
  f_async_service_ <<
    async_client_class_name << "::" << async_client_class_name << "() : " << client_base_class_name << "() {" << endl;
  f_async_service_ <<
    indent() << "client_.reset(new " << client_class_name << "(input_proto_, output_proto_));" << endl;
  f_async_service_ << "}" << endl << endl;

  f_async_service_ <<
    async_client_class_name << "::" << async_client_class_name <<
    "(const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket) : " << client_base_class_name << "(socket) {" << endl;
  f_async_service_ <<
    indent() << "client_.reset(new " << client_class_name << "(input_proto_, output_proto_));" << endl;
  f_async_service_ << "}" << endl << endl;

  f_async_service_ <<
    async_client_class_name << "::~" << async_client_class_name << "() {}" << endl << endl;

  //function(.cpp)
  for (size_t i=0; i<functions.size(); i++)
  {
    t_function * function = functions[i];
    t_type * ret_type = function->get_returntype();

    //async RPC
    f_async_service_ <<
      async_if_function_signature(function, async_client_class_name + "::") << " {" << endl;

    f_async_service_ <<
      indent() << "if (!is_open())" << endl;
    indent_up();
    f_async_service_ <<
      indent() << "boost::system::error_code ec(boost::system::posix_error::not_connected, boost::system::get_posix_category());"
      << endl << endl;
    indent_down();

    f_async_service_ <<
      indent() << "uint32_t out_frame_size;" << endl <<
      indent() << "uint8_t * out_frame;" << endl <<
      indent() << "boost::system::error_code ec;" << endl << endl;

    f_async_service_ <<
      indent() << "boost::shared_ptr<AsyncOp> op(new AsyncOp);" << endl <<
      indent() << "async_op_list_.push_back(op);" << endl <<
      indent() << "op->callback = callback;" << endl <<
      indent() << "op->rpc_type = " << function_op_enums[i] << ";" << endl;

    if (!ret_type->is_void())
      f_async_service_ <<
      indent() << "op->_return = static_cast<void*>(&_return);" << endl;
    else
      f_async_service_ <<
      indent() << "op->_return = NULL;" << endl;

    if (function->is_oneway())
      f_async_service_ <<
      indent() << "op->is_oneway = true;" << endl << endl;
    else
      f_async_service_ <<
      indent() << "op->is_oneway = false;" << endl << endl;

    f_async_service_ <<
      indent() << "pending_async_op_ = op;" << endl << endl;

    const vector<t_field*>& args = function->get_arglist()->get_members();
    string arg_string;
    for (size_t i=0; i<args.size(); i++)
    {
      arg_string += args[i]->get_name();
      if (i != args.size()-1)
        arg_string += ", ";
    }

    f_async_service_ <<
      indent() << "output_buffer_->resetBuffer();" << endl <<
      indent() << "client_->send_" << function->get_name() << "(" << arg_string <<");" << endl <<
      indent() << "output_buffer_->getBuffer(&out_frame, &out_frame_size);" << endl << endl;

    //async_write
    f_async_service_ <<
      indent() << "if (strand_)" << endl;

    indent_up();
    f_async_service_ <<
      indent() << "boost::asio::async_write(*socket_," << endl;
    indent_up();
    f_async_service_ <<
      indent() << "boost::asio::buffer(out_frame, out_frame_size)," << endl;
    f_async_service_ <<
      indent() << "boost::asio::transfer_all()," << endl;
    f_async_service_ <<
      indent() << "strand_->wrap(boost::bind(&" << async_client_class_name << "::handle_write, this, " <<
      "_1, _2)));" << endl;
    indent_down();
    indent_down();

    f_async_service_ <<
      indent() << "else" << endl;

    indent_up();
    f_async_service_ <<
      indent() << "boost::asio::async_write(*socket_," << endl;
    indent_up();
    f_async_service_ <<
      indent() << "boost::asio::buffer(out_frame, out_frame_size)," << endl;
    f_async_service_ <<
      indent() << "boost::asio::transfer_all()," << endl;
    f_async_service_ <<
      indent() << "boost::bind(&" << async_client_class_name << "::handle_write, this, " <<
      "_1, _2));" << endl;
    indent_down();
    indent_down();
    //end of async_write

    f_async_service_ << "}" << endl << endl;

    //sync RPC
    f_async_service_ <<
      function_signature(function, async_client_class_name + "::") << " {" << endl;

    f_async_service_ <<
      indent() << "using ::apache::thrift::GlobalOutput;" << endl << endl;

    f_async_service_ <<
      indent() << "if (!is_open())" << endl;
    indent_up();
    f_async_service_ <<
      indent() << "boost::system::error_code ec(boost::system::posix_error::not_connected, boost::system::get_posix_category());"
      << endl << endl;
    indent_down();

    f_async_service_ <<
      indent() << "uint32_t out_frame_size;" << endl <<
      indent() << "uint8_t * out_frame;" << endl <<
      indent() << "boost::system::error_code ec;" << endl << endl;

    f_async_service_ <<
      indent() << "output_buffer_->resetBuffer();" << endl <<
      indent() << "client_->send_" << function->get_name() << "(" << arg_string <<");" << endl <<
      indent() << "output_buffer_->getBuffer(&out_frame, &out_frame_size);" << ";" << endl << endl;

    f_async_service_ <<
      indent() << "boost::asio::write(*socket_," << endl;
    indent_up();
    f_async_service_ <<
      indent() << "boost::asio::buffer(out_frame, out_frame_size)," << endl <<
      indent() << "boost::asio::transfer_all(), ec);" << endl << endl;
    indent_down();

    f_async_service_ <<
      indent() << "if (ec) {" << endl;
    indent_up();
    f_async_service_ <<
      indent() << "close();" << endl <<
      indent() << "GlobalOutput.printf(\"%s caught an error code: %s\", __FUNCTION__, ec.message().c_str());" << endl <<
      indent() << "throw boost::system::system_error(ec);" << endl;
    indent_down();
    f_async_service_ <<
      indent() << "}" << endl;

    if (!function->is_oneway())
    {
      f_async_service_ << endl;

      f_async_service_ <<
        indent() << "recv_buffer_.resize(sizeof(int32_t));" << endl <<
        indent() << "boost::asio::read(*socket_, boost::asio::buffer(recv_buffer_), boost::asio::transfer_all(), ec);" << endl;
      f_async_service_ <<
        indent() << "if (ec) {" << endl;
      indent_up();
      f_async_service_ <<
        indent() << "close();" << endl <<
        indent() << "GlobalOutput.printf(\"%s caught an error code: %s\", __FUNCTION__, ec.message().c_str());" << endl <<
        indent() << "throw boost::system::system_error(ec);" << endl;
      indent_down();
      f_async_service_ <<
        indent() << "}" << endl;

      f_async_service_ <<
        indent() << "get_frame_size();" << endl << endl <<
        indent() << "if (frame_size_ <= 0) {" << endl;
      indent_up();
      f_async_service_ <<
        indent() << "close();" << endl <<
        indent() << "GlobalOutput.printf(\"%s frame size <= 0: %d\", __FUNCTION__, frame_size_);" << endl <<
        indent() << "throw ::apache::thrift::transport::TTransportException(\"frame size <= 0\");" << endl;
      indent_down();
      f_async_service_ <<
        indent() << "}" << endl << endl;

      f_async_service_ <<
        indent() << "recv_buffer_.resize(sizeof(int32_t) + frame_size_);" << endl <<
        indent() << "boost::asio::read(*socket_, boost::asio::buffer(&recv_buffer_[0] + sizeof(int32_t), frame_size_), "
        "boost::asio::transfer_all(), ec);" << endl << endl <<
        indent() << "if (ec) {" << endl;
      indent_up();
      f_async_service_ <<
        indent() << "close();" << endl <<
        indent() << "GlobalOutput.printf(\"%s caught an error code: %s\", __FUNCTION__, ec.message().c_str());" << endl <<
        indent() << "throw boost::system::system_error(ec);" << endl;
      indent_down();
      f_async_service_ <<
        indent() << "}" << endl << endl;

      f_async_service_ <<
        indent() << "input_buffer_->resetBuffer(&recv_buffer_[0], recv_buffer_.size());" << endl << endl;

      if (is_complex_type(ret_type))
      {
        f_async_service_ <<
          indent() << "client_->recv_" << function->get_name() << "(_return);" << endl;
      }
      else if (!ret_type->is_void())
      {
        f_async_service_ <<
          indent() << "return client_->recv_" << function->get_name() << "();" << endl;
      }
      else
      {
        f_async_service_ <<
          indent() << "client_->recv_" << function->get_name() << "();" << endl;
      }
    }

    f_async_service_ <<
      "}" << endl << endl;
  }

  //key virtual function: fill_result(.cpp)
  f_async_service_ <<
    "void " << async_client_class_name << "::fill_result(AsyncOp& op) {" << endl;

  f_async_service_ <<
    indent() << "switch (op.rpc_type) {" << endl;

  for (size_t i=0; i<functions.size(); i++)
  {
    t_function * function = functions[i];
    t_type * ret_type = function->get_returntype();

    if (!ret_type->is_void())//including oneway
    {
      f_async_service_ <<
        indent() << "case " << function_op_enums[i] << ":" << endl;
      indent_up();
      if (is_complex_type(ret_type))
      {
        f_async_service_ <<
          indent() << "client_->recv_" << function->get_name() <<
          "(*(static_cast<" << type_name(ret_type) << "*>(op._return)));" << endl <<
          indent() << "break;" << endl;
      }
      else
      {
        f_async_service_ <<
          indent() << "(*(static_cast<" << type_name(ret_type) << "*>(op._return))) = client_->recv_"
          << function->get_name() << "();" << endl <<
          indent() << "break;" << endl;
      }
      indent_down();
    }
  }
  if (base_tservice)
  {
    f_async_service_ <<
      indent() << "default:" << endl;
    indent_up();
    f_async_service_ <<
      indent() << base_async_client_class_name << "::fill_result(op);" << endl <<
      indent() << "break;" << endl;
    indent_down();
  }

  f_async_service_ <<
    indent() << "}" << endl;

  f_async_service_ << "}" << endl << endl;
  indent_down();
}


void t_cpp_generator::generate_async_if_and_processor(t_service* tservice)
{
  string svcname = tservice->get_name();
  t_service * base_tservice = tservice->get_extends();
  string async_base_svcname;
  if (base_tservice)
    async_base_svcname = "Async" + base_tservice->get_name();


  string if_class_name = svcname + "If";
  string async_if_class_name = "Async" + svcname + "If";
  string async_null_if_class_name = "Async" + svcname + "Null";
  string async_adapter_if_class_name = "Async" + svcname + "Adapter";
  string async_processor_class_name = "Async" + svcname + "Processor";
  string base_if_class_name;
  string async_base_if_class_name;
  string async_base_null_if_class_name;
  string async_base_adapter_if_class_name;
  string async_base_processor_class_name;
  if (base_tservice)
  {
    base_if_class_name = base_tservice->get_name() + "If";
    async_base_if_class_name = "Async" + base_tservice->get_name() + "If";
    async_base_null_if_class_name = "Async" + base_tservice->get_name() + "Null";
    async_base_adapter_if_class_name = "Async" + base_tservice->get_name() + "Adapter";
    async_base_processor_class_name = "Async" + base_tservice->get_name() + "Processor";
  }

  vector<t_function*> functions = tservice->get_functions();

  /************************************************************************/

  indent_up();
  //class AsyncIF header(.h)
  f_async_header_ << "class " << async_if_class_name << endl;
  if (base_tservice)
  {
    f_async_header_ << indent() << ": virtual public " <<
      namespace_prefix(base_tservice->get_program()->get_namespace("cpp")) << async_base_if_class_name << endl;
  }
  f_async_header_ << "{" << endl;
  f_async_header_ << "public:" << endl;

  //destructor
  f_async_header_ << indent() << "virtual ~" << async_if_class_name << "() {}" << endl;
  f_async_header_ << endl;

  //function(.h)
  for (size_t i=0; i<functions.size(); i++)
  {
    t_function * function = functions[i];
    //async if
    f_async_header_ << indent() << async_if_function_signature(function) << " = 0;" << endl;
  }

  //class AsyncIF end(.h)
  f_async_header_ <<
    "};" << endl << endl;
  indent_down();

  /************************************************************************/

  indent_up();
  //class AsyncNull header(.h)
  f_async_header_ << "class " << async_null_if_class_name << endl;
  f_async_header_ << indent() << ": virtual public " << async_if_class_name;
  if (base_tservice)
  {
    f_async_header_ << "," << endl << indent() << "virtual public " <<
      namespace_prefix(base_tservice->get_program()->get_namespace("cpp")) << async_base_null_if_class_name << endl;
  }
  else
  {
    f_async_header_ << endl;
  }
  f_async_header_ << "{" << endl;
  f_async_header_ << "public:" << endl;

  //destructor
  f_async_header_ << indent() << "virtual ~" << async_null_if_class_name << "() {}" << endl;
  f_async_header_ << endl;

  //function(.h)
  for (size_t i=0; i<functions.size(); i++)
  {
    t_function * function = functions[i];
    f_async_header_ << indent() << async_if_function_signature(function) << " {"<< endl;
    indent_up();
    f_async_header_ << indent() << "callback(boost::system::error_code());" << endl;
    indent_down();
    f_async_header_ << indent() << "}"<< endl;

    if (i != functions.size() - 1)
      f_async_header_ << endl;
  }

  //class AsyncNull end(.h)
  f_async_header_ <<
    "};" << endl << endl;
  indent_down();

  /************************************************************************/

  indent_up();
  f_async_header_ << "// This class is used to adapt a synchronous handler to an asynchronous one" << endl;
  //class AsyncAdapter header(.h)
  f_async_header_ << "class " << async_adapter_if_class_name << endl;
  f_async_header_ << indent() << ": virtual public " << async_if_class_name;
  if (base_tservice)
  {
    f_async_header_ << "," << endl << indent() << "virtual public " <<
      namespace_prefix(base_tservice->get_program()->get_namespace("cpp")) << async_base_adapter_if_class_name << endl;
  }
  else
  {
    f_async_header_ << endl;
  }
  f_async_header_ << "{" << endl;

  f_async_header_ << "private:" << endl;
  f_async_header_ << indent() << "boost::shared_ptr<" << if_class_name << "> sync_if_;"<< endl;
  f_async_header_ << endl;

  f_async_header_ << "public:" << endl;

  //constructor
  f_async_header_ << indent() << "explicit " << async_adapter_if_class_name << "(const boost::shared_ptr<" << if_class_name << ">& sync_if)"<< endl;
  indent_up();
  if (base_tservice)
  {
    f_async_header_ << indent() << ": " << async_base_adapter_if_class_name
      << "(boost::dynamic_pointer_cast<" << namespace_prefix(base_tservice->get_program()->get_namespace("cpp")) << base_if_class_name
      << ", " << if_class_name << ">(sync_if))," << endl;

    f_async_header_ << indent() << "sync_if_(sync_if)" << endl;
  }
  else
  {
    f_async_header_ << indent() << ": sync_if_(sync_if)" << endl;
  }
  indent_down();
  f_async_header_ << indent() << "{}" << endl;

  //destructor
  f_async_header_ << indent() << "virtual ~" << async_adapter_if_class_name << "() {}" << endl;
  f_async_header_ << endl;

  //function(.h)
  for (size_t i=0; i<functions.size(); i++)
  {
    t_function * function = functions[i];
    t_type * ret_type = function->get_returntype();
    t_struct * arglist = function->get_arglist();

    f_async_header_ << indent() << async_if_function_signature(function) << " {"<< endl;
    indent_up();

    if (is_complex_type(ret_type))
    {
      f_async_header_ << indent() << "sync_if_->" << function->get_name() << "(";

      f_async_header_ << "_return";
      if (!arglist->get_members().empty())
        f_async_header_ << ", ";

      for (size_t j=0; j<arglist->get_members().size(); j++)
      {
        t_field * field = arglist->get_members()[j];
        f_async_header_ << field->get_name();
        if (j != arglist->get_members().size() - 1)
          f_async_header_ << ", ";
      }

      f_async_header_ << ");" << endl;
    }
    else if (!ret_type->is_void())
    {
      f_async_header_ << indent() << "_return = sync_if_->" << function->get_name() << "(";

      for (size_t j=0; j<arglist->get_members().size(); j++)
      {
        t_field * field = arglist->get_members()[j];
        f_async_header_ << field->get_name();
        if (j != arglist->get_members().size() - 1)
          f_async_header_ << ", ";
      }

      f_async_header_ << ");" << endl;
    }
    else
    {
      f_async_header_ << indent() << "sync_if_->" << function->get_name() << "(";

      for (size_t j=0; j<arglist->get_members().size(); j++)
      {
        t_field * field = arglist->get_members()[j];
        f_async_header_ << field->get_name();
        if (j != arglist->get_members().size() - 1)
          f_async_header_ << ", ";
      }

      f_async_header_ << ");" << endl;
    }

    f_async_header_ << indent() << "callback(boost::system::error_code());" << endl;
    indent_down();
    f_async_header_ << indent() << "}"<< endl;

    if (i != functions.size() - 1)
      f_async_header_ << endl;
  }

  //class AsyncAdapter end(.h)
  f_async_header_ <<
    "};" << endl << endl;
  indent_down();

  /************************************************************************/

  indent_up();
  //class AsyncProcessor header(.h)
  f_async_header_ << "class " << async_processor_class_name << endl;
  f_async_header_ << indent() << ": virtual public ::apache::thrift::async::AsyncProcessor";
  if (base_tservice)
  {
    f_async_header_ << "," << endl << indent() << "public " <<
      namespace_prefix(base_tservice->get_program()->get_namespace("cpp")) << async_base_processor_class_name << endl;
  }
  else
  {
    f_async_header_ << endl;
  }
  f_async_header_ << "{" << endl;

  f_async_header_ << "protected:" << endl;
  //process_fn(.h)
  f_async_header_ << indent() << "virtual void process_fn(" << endl;
  indent_up();
  f_async_header_ << indent() << "::apache::thrift::protocol::TProtocol * input_protocol," << endl;
  f_async_header_ << indent() << "::apache::thrift::protocol::TProtocol * output_protocol," << endl;
  f_async_header_ << indent() << "::apache::thrift::async::AsyncProcessorCallback callback," << endl;
  f_async_header_ << indent() << "std::string& fname, int32_t seqid);" << endl;
  indent_down();
  f_async_header_ << endl;

  f_async_header_ << "private:" << endl;
  //handler_(.h)
  f_async_header_ << indent() << "boost::shared_ptr<" << async_if_class_name << "> handler_;" << endl;
  f_async_header_ << endl;

  //FunctionMap(.h)
  f_async_header_ << indent() << "typedef std::map<std::string," << endl;
  indent_up();
  f_async_header_ << indent() << "void ("<< async_processor_class_name << "::*)(" << endl;
  f_async_header_ << indent() << "int32_t, ::apache::thrift::async::AsyncProcessorCallback," << endl;
  f_async_header_ << indent() << "::apache::thrift::protocol::TProtocol *," << endl;
  f_async_header_ << indent() << "::apache::thrift::protocol::TProtocol *)> FunctionMap;" << endl;
  indent_down();
  f_async_header_ << indent() << "FunctionMap process_fn_map_;" << endl;
  f_async_header_ << endl;

  //function(.h)
  for (size_t i=0; i<functions.size(); i++)
  {
    t_function * function = functions[i];
    f_async_header_ << indent() << async_process_function_signature(function) << ";" << endl;
    f_async_header_ << indent() << async_complete_function_signature(function, svcname) << ";" << endl;
    f_async_header_ << endl;
  }

  f_async_header_ << "public:" << endl;
  //ctor and dtor(.h)
  f_async_header_ << indent() << async_processor_class_name <<
    "(const boost::shared_ptr<" << async_if_class_name << ">& handler)" << endl;
  indent_up();
  if (base_tservice)
    f_async_header_ << indent() << ": " <<
    namespace_prefix(base_tservice->get_program()->get_namespace("cpp")) << async_base_processor_class_name <<
    "(handler), handler_(handler) {" << endl;
  else
    f_async_header_ << indent() << ": handler_(handler) {" << endl;
  for (size_t i=0; i<functions.size(); i++)
  {
    t_function * function = functions[i];
    f_async_header_ << indent() << "process_fn_map_[\"" << function->get_name() << "\"] = &" <<
      async_processor_class_name << "::process_" << function->get_name() << ";" << endl;
  }
  indent_down();
  f_async_header_ << indent() << "}" << endl << endl;

  f_async_header_ << indent() << "virtual ~" << async_processor_class_name << "() {}" << endl;

  //class AsyncProcessor end(.h)
  f_async_header_ <<
    "};" << endl << endl;
  indent_down();

  /************************************************************************/

  //process_fn(.cpp)
  indent_up();
  f_async_service_ << "void " << async_processor_class_name << "::process_fn(" << endl;
  f_async_service_ << indent() << "::apache::thrift::protocol::TProtocol * input_protocol," << endl;
  f_async_service_ << indent() << "::apache::thrift::protocol::TProtocol * output_protocol," << endl;
  f_async_service_ << indent() << "::apache::thrift::async::AsyncProcessorCallback callback," << endl;
  f_async_service_ << indent() << "std::string& fname, int32_t seqid) {" << endl;
  f_async_service_ << indent() << "FunctionMap::iterator pfn = process_fn_map_.find(fname);" << endl;
  f_async_service_ << indent() << "if (pfn == process_fn_map_.end()) {" << endl;
  indent_up();
  if (base_tservice)
  {
    f_async_service_ << indent() << async_base_processor_class_name <<
      "::process_fn(input_protocol, output_protocol, callback, fname, seqid);" << endl;
    f_async_service_ << indent() << "return;" << endl;
  }
  else
  {
    f_async_service_ << indent() << "input_protocol->skip(::apache::thrift::protocol::T_STRUCT);" << endl;
    f_async_service_ << indent() << "input_protocol->readMessageEnd();" << endl;
    f_async_service_ << indent() << "input_protocol->getTransport()->readEnd();" << endl;
    f_async_service_ << indent() << "::apache::thrift::TApplicationException x(::apache::thrift::TApplicationException::UNKNOWN_METHOD, \"Invalid method name: '\"+fname+\"'\");" << endl;
    f_async_service_ << indent() << "output_protocol->writeMessageBegin(fname, ::apache::thrift::protocol::T_EXCEPTION, seqid);" << endl;
    f_async_service_ << indent() << "x.write(output_protocol);" << endl;
    f_async_service_ << indent() << "output_protocol->writeMessageEnd();" << endl;
    f_async_service_ << indent() << "output_protocol->getTransport()->flush();" << endl;
    f_async_service_ << indent() << "output_protocol->getTransport()->writeEnd();" << endl;
    f_async_service_ << indent() << "boost::system::error_code ec(boost::system::posix_error::bad_message, boost::system::get_posix_category());" << endl;
    f_async_service_ << indent() << "callback(ec, false);" << endl;
    f_async_service_ << indent() << "return;" << endl;
  }
  indent_down();
  f_async_service_ << indent() << "}" << endl;
  f_async_service_ << indent() << "(this->*(pfn->second))(seqid, callback, input_protocol, output_protocol);" << endl;
  f_async_service_ << "}" << endl << endl;
  indent_down();

  //function(.cpp)
  indent_up();
  for (size_t i=0; i<functions.size(); i++)
  {
    t_function * function = functions[i];
    t_type * ret_type = function->get_returntype();
    t_struct * arglist = function->get_arglist();
    bool is_oneway = function->is_oneway();
    bool ret_type_is_void = ret_type->is_void();
    bool arglist_is_empty = arglist->get_members().empty();

    string args_class = svcname + "_" + function->get_name() + "_args";
    string result_class = svcname + "_" + function->get_name() + "_result";
    string args = function->get_name() + "_args";
    string result = function->get_name() + "_result";


    f_async_service_ << async_process_function_signature(function, async_processor_class_name+"::") << " {" << endl;
    f_async_service_ << indent() << "boost::shared_ptr<" << args_class << "> " << args << "(new " << args_class << ");" << endl;
    if (!is_oneway)
      f_async_service_ << indent() << "boost::shared_ptr<" << result_class << "> " << result << "(new " << result_class << ");" << endl;
    f_async_service_ << indent() << args << "->read(input_protocol);" << endl;
    f_async_service_ << indent() << "input_protocol->readMessageEnd();" << endl;
    f_async_service_ << indent() << "input_protocol->getTransport()->readEnd();" << endl;
    f_async_service_ << indent() << "handler_->async_" << function->get_name() << "(" << endl;
    indent_up();
    if (!ret_type_is_void)
      f_async_service_ << indent() << result << "->success," << endl;
    if (!arglist_is_empty)
    {
      for (size_t i=0; i<arglist->get_members().size(); i++)
      {
        t_field * field = arglist->get_members()[i];
        f_async_service_ << indent() << args << "->" << field->get_name() << "," << endl;
      }
    }
    f_async_service_ << indent() << "boost::bind(&" << async_processor_class_name << "::complete_" << function->get_name() << "," << endl;
    f_async_service_ << indent() << "this, seqid, callback, input_protocol, output_protocol, " << args;
    if (!is_oneway)
      f_async_service_ << ", " << result;
    f_async_service_ << ", _1));" << endl;
    indent_down();
    f_async_service_ << "}" << endl << endl;


    f_async_service_ << async_complete_function_signature(function, svcname, async_processor_class_name+"::") << " {" << endl;
    if (is_oneway)
    {
      f_async_service_ << indent() << "callback(ec, true);" << endl;
    }
    else
    {
      f_async_service_ << indent() << "if (ec) {" << endl;
      indent_up();
      f_async_service_ << indent() << "::apache::thrift::TApplicationException x(ec.message());" << endl;
      f_async_service_ << indent() << "output_protocol->writeMessageBegin(\"" <<
        function->get_name() << "\", ::apache::thrift::protocol::T_EXCEPTION, seqid);" << endl;
      f_async_service_ << indent() << "x.write(output_protocol);" << endl;
      f_async_service_ << indent() << "output_protocol->writeMessageEnd();" << endl;
      f_async_service_ << indent() << "output_protocol->getTransport()->flush();" << endl;
      f_async_service_ << indent() << "output_protocol->getTransport()->writeEnd();" << endl;
      indent_down();
      f_async_service_ << indent() << "}" << endl;

      f_async_service_ << indent() << "else {" << endl;
      indent_up();
      if (!ret_type_is_void)
        f_async_service_ << indent() << result << "->__isset.success = true;" << endl;
      f_async_service_ << indent() << "output_protocol->writeMessageBegin(\"" <<
        function->get_name() << "\", ::apache::thrift::protocol::T_REPLY, seqid);" << endl;
      f_async_service_ << indent() << result << "->write(output_protocol);" << endl;
      f_async_service_ << indent() << "output_protocol->writeMessageEnd();" << endl;
      f_async_service_ << indent() << "output_protocol->getTransport()->flush();" << endl;
      f_async_service_ << indent() << "output_protocol->getTransport()->writeEnd();" << endl;
      indent_down();
      f_async_service_ << indent() << "}" << endl;
      f_async_service_ << indent() << "callback(ec, false);" << endl;
    }

    f_async_service_ << "}" << endl << endl;
  }
  indent_down();
}

protoc auth_session_message.proto --cpp_out=.
protoc desktop_session_message.proto --cpp_out=.
protoc power_session_message.proto --cpp_out=.
protoc file_transfer_session_message.proto --cpp_out=.

protoc auth_session.proto --cpp_out=.
protoc desktop_session.proto --cpp_out=.
protoc power_session.proto --cpp_out=.
protoc file_transfer_session.proto --cpp_out=.

protoc host_user.proto --cpp_out=.

protoc key_exchange.proto --cpp_out=.
pause
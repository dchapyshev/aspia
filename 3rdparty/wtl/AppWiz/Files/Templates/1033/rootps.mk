
[!output PROJECT_NAME]ps.dll: dlldata.obj [!output PROJECT_NAME]_p.obj [!output PROJECT_NAME]_i.obj
	link /dll /out:[!output PROJECT_NAME]ps.dll /def:[!output PROJECT_NAME]ps.def /entry:DllMain dlldata.obj [!output PROJECT_NAME]_p.obj [!output PROJECT_NAME]_i.obj \
		kernel32.lib rpcndr.lib rpcns4.lib rpcrt4.lib oleaut32.lib uuid.lib \

.c.obj:
	cl /c /Ox /DWIN32 /D_WIN32_WINNT=0x0400 /DREGISTER_PROXY_DLL \
		$<

clean:
	@del [!output PROJECT_NAME]ps.dll
	@del [!output PROJECT_NAME]ps.lib
	@del [!output PROJECT_NAME]ps.exp
	@del dlldata.obj
	@del [!output PROJECT_NAME]_p.obj
	@del [!output PROJECT_NAME]_i.obj

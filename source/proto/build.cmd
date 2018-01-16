for %%f in (*.proto) do (
	protoc %%f --cpp_out=.
)
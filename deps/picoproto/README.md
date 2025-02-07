# picoproto
Abominably Tiny Protobuf File Parser in C++

## What is it?

Picoproto is a single file C++ implementation of protocol buffer parsing. It's 
designed to have no dependencies other than the standard library available in 
C++11, and to compile to a small binary size.

## Why did I build it?

Protocol Buffers can make large projects a lot easier by using a common language
to describe data objects. Unfortunately this comes at a cost in executable size 
and complexity that can be too much for resource-limited platforms. On a 
reasonably-sized project, the base protobuf libraries and the access classes 
created for each type can easily take up several hundred kilobytes.

This frustrated me, because I wanted to retain the ease of data transfer that 
protobufs offer, even on systems that can't afford increases in executable size.
In many cases, all I want to do is treat protobuf as a file format, and read 
data from those files as efficiently as possible. I don't need to write out 
protobuf objects, or have automatically-created access classes for each type. I 
also don't want to do any processing of .proto files at all, since that adds 
build complexity which can cause a lot of headaches when moving around 
platforms.

## How do you use it?

The main class is Message. This is a key/value store that holds the parsed
contents of a protobuf form. The keys are the field numbers that were defined in
a .proto file, and the values are the contents of each field.

To start, create an empty Message object:

```c++
picoproto::Message message;
```

Then load a protobuf file into an array of raw bytes:
```c++
FILE* f = fopen("somefile.pb", "rb");
fseek(f, 0, SEEK_END);
long fsize = ftell(f);
fseek(f, 0, SEEK_SET);
uint8_t* bytes = new uint8_t[fsize];
fread(bytes, fsize, 1, f);
fclose(f);
```

Deserialize the contents of those bytes into the Message:

```c++
message.ParseFromBytes(bytes, fsize);
```

The message object will now contain all of the fields that were stored in the
protobuf file. To access them, you need to know the number and type of each
field. This is a big difference from the full protobuf library, because there
you can use convenient functions to access any field by its name. To keep the
implementation simple, picoproto doesn't offer this convenience.

For example, if you had loaded a proto created with this definition:

```
message Test1 {
  required int32 a = 1;
}
```

Then you would access the `a` member like this:

```c++
int32_t a = message.GetInt32(1);
```

For a repeated or optional field, you would use the array accessors:

```
message Test2 {
  repeated string name = 7;
}
```

```c++
std::vector<string> names = message.GetStringArray(7);
```

Embedded messages are fully supported, and can be accessed using similar
functions:

```
message Test3 {
  required Test1 c = 3;
}
```

```c++
Message* c = message.GetMessage(3);
int32_t a = c->GetInt32(1);
```

For more the examples, see `picoproto_test.cc` and `tf_to_dot.cc`.

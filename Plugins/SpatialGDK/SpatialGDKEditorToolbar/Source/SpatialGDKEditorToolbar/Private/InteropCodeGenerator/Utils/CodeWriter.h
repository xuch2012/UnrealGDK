// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

struct FFunctionSignature
{
	FString Type;
	FString NameAndParams;

	FString Declaration() const
	{
		return FString::Printf(TEXT("%s %s;"), *Type, *NameAndParams);
	}

	FString Definition() const
	{
		return FString::Printf(TEXT("%s %s"), *Type, *NameAndParams);
	}

	FString Definition(const FString& TypeName) const
	{
		return FString::Printf(TEXT("%s %s::%s"), *Type, *TypeName, *NameAndParams);
	}
};

class FCodeWriter
{
public:
	FCodeWriter();

	TArray<FString> Imports;
	TArray<FString> SingleClientRepData;
	TArray<FString> MultiClientRepData;
	TArray<FString> ClientRPCRepData;
	TArray<FString> ServerRPCRepData;
	TArray<FString> MulticastRPCRepData;

	template <typename... T>
	FCodeWriter& Printf(const FString& Format, const T&... Args)
	{
		return Print(FString::Printf(*Format, Args...));
	}

	void AddImport(FString Import)
	{
		Imports.Add(Import);
	}

	void AddSingleClientRepDataField(FString RepDataFieldString)
	{
		SingleClientRepData.Add(RepDataFieldString);
	}

	void AddMultiClientRepDataField(FString RepDataFieldString)
	{
		MultiClientRepData.Add(RepDataFieldString);
	}

	void AddClientRPCRepDataField(FString RepDataFieldString)
	{
		ClientRPCRepData.Add(RepDataFieldString);
	}

	void AddServerRPCRepDataField(FString RepDataFieldString)
	{
		ServerRPCRepData.Add(RepDataFieldString);
	}

	void AddMulticastRPCRepDataField(FString RepDataFieldString)
	{
		MulticastRPCRepData.Add(RepDataFieldString);
	}

	FCodeWriter& PrintNewLine();
	FCodeWriter& Print(const FString& String);
	FCodeWriter& Indent();
	FCodeWriter& Outdent();

	FCodeWriter& BeginScope();
	FCodeWriter& BeginFunction(const FFunctionSignature& Signature);
	FCodeWriter& BeginFunction(const FFunctionSignature& Signature, const FString& TypeName);
	FCodeWriter& End();

	void WriteToFile(const FString& Filename);
	void Dump();

	FCodeWriter(const FCodeWriter& other) = delete;
	FCodeWriter& operator=(const FCodeWriter& other) = delete;

private:
	FString OutputSource;
	int Scope;
};

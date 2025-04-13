//Copyright 2022~2024 DevGrain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SharedType/JointSharedTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "JointFunctionLibrary.generated.h"

class UWidget;
/**
 * 
 */
UCLASS()
class JOINT_API UJointFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	/*
	 * Check if the provided widget is focusable.
	 */
	UFUNCTION(BlueprintCallable, Category="Condition")
	static bool IsWidgetFocusable(UWidget* TargetWidget);


	/*
	 * Format the provided text with the provided format argument map 
	 */
	UFUNCTION(BlueprintCallable, Category="Joint Text Utilities")
	static FText FormatTextWithMap(FText InText, TMap<FString, FText> Map);
	
	/*
	 * Format the provided text with the provided format argument. 
	 */
	UFUNCTION(BlueprintCallable, Category="Joint Text Utilities")
	static FText FormatTextWith(FText InText, FString Target, FText Format);


	
	/**
	 * Get every range that the content texts take place, including empty run's content.
	 */
	UFUNCTION(BlueprintCallable, Category="Joint Text Utilities")
	static TArray<FInt32Range> GetTextContentRange(FText InText);

	/**
	 * Get every range that the decorator symbols take place. The result will be a complement of the GetTextContentRange() ranges.
	 */
	UFUNCTION(BlueprintCallable, Category="Joint Text Utilities")
	static TArray<FInt32Range> GetDecoratorSymbolRange(FText InText);
	
	/**
	 * Get every range that the decorated run's content texts take places. This excludes the empty run's content.
	 */
	UFUNCTION(BlueprintCallable, Category="Joint Text Utilities")
	static TArray<FInt32Range> GetDecoratedTextContentRange(FText InText);
	
	/**
	 * Merge text style data tables. If there is redundant row name, it will use the first case.
	 * It only works with the table with FRichTextStyleRow row struct. if you use some custom type of it then You must try to make one custom version of this function for your project.
	 * Note: We Highly recommended to set the row name with specific label on them.
	 * for example, RichText.Cute.Default, RichText.Cute.Row1, RichText.Cute.Row2, RichText.Cute.Row3... like this.
	 * @param TablesToMerge tables to merge together.
	 * @return A merged table instance. Notice this instance will be transient, can not be stored and serialized.
	 */
	UFUNCTION(BlueprintCallable, Category="Joint Text Utilities")
	static UDataTable* MergeTextStyleDataTables(TSet<UDataTable*> TablesToMerge);

public:

	//Pin Related

	/**
	 * Implement pin while trying to maintain existing pins.
	 * @param ExistingPins Already existing pins. If NeededPinSignature array has pins that has the same signatures, they will be maintained, otherwise, discarded.
	 * @param NeededPinSignature Total list of Pins we need to implement. if ExistingPins array already has pins that we need, it will use that instead of implementing it newly.
	 * @return New Total pins.
	 */
	UFUNCTION(BlueprintPure, BlueprintCallable, Category="Joint Text Utilities")
	static TArray<FJointEdPinData> ImplementPins(const TArray<FJointEdPinData>& ExistingPins,const TArray<FJointEdPinData>& NeededPinSignature);


	UFUNCTION(BlueprintPure, BlueprintCallable, Category="Joint Text Utilities")
	static const bool AreBothPinHaveSameSignature(const FJointEdPinData& A, const FJointEdPinData& B);

	
	
};

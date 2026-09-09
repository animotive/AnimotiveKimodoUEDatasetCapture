// Copyright Animotive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneTrack.h"

#include "MovieSceneDatasetInputTrack.generated.h"

class UMovieSceneDatasetInputSection;

/**
 * The input track added to a take's LevelSequence alongside the recorded animation.
 *
 * One track, one section per take. The track exists so the input data is a first-class part
 * of the sequence: it saves with the take, scrubs with the take, and cannot be separated
 * from the animation it describes.
 */
UCLASS(MinimalAPI)
class UMovieSceneDatasetInputTrack : public UMovieSceneTrack
{
	GENERATED_BODY()

public:
	KIMODODATASETCAPTURE_API UMovieSceneDatasetInputTrack(const FObjectInitializer& ObjectInitializer);

	/** The track's single input section, creating it if the track is empty. */
	KIMODODATASETCAPTURE_API UMovieSceneDatasetInputSection* GetOrCreateInputSection();

	/** The track's input section, or null when the track is empty. */
	KIMODODATASETCAPTURE_API UMovieSceneDatasetInputSection* GetInputSection() const;

	//~ Begin UMovieSceneTrack
	KIMODODATASETCAPTURE_API virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
	KIMODODATASETCAPTURE_API virtual UMovieSceneSection* CreateNewSection() override;
	KIMODODATASETCAPTURE_API virtual void AddSection(UMovieSceneSection& Section) override;
	KIMODODATASETCAPTURE_API virtual void RemoveSection(UMovieSceneSection& Section) override;
	KIMODODATASETCAPTURE_API virtual void RemoveSectionAt(int32 SectionIndex) override;
	KIMODODATASETCAPTURE_API virtual bool HasSection(const UMovieSceneSection& Section) const override;
	KIMODODATASETCAPTURE_API virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	KIMODODATASETCAPTURE_API virtual bool IsEmpty() const override;

#if WITH_EDITORONLY_DATA
	KIMODODATASETCAPTURE_API virtual FText GetDisplayName() const override;
#endif
	//~ End UMovieSceneTrack

private:
	UPROPERTY()
	TArray<UMovieSceneSection*> Sections;
};

// Copyright Animotive. All Rights Reserved.

#include "MovieSceneDatasetInputTrack.h"

#include "MovieSceneDatasetInputSection.h"

#define LOCTEXT_NAMESPACE "MovieSceneDatasetInputTrack"

UMovieSceneDatasetInputTrack::UMovieSceneDatasetInputTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	// Distinct from the animation track's colour so an operator can tell at a glance which
	// row carries input and which carries motion.
	SetColorTint(FColor(90, 140, 200, 65));
#endif
}

UMovieSceneDatasetInputSection* UMovieSceneDatasetInputTrack::GetOrCreateInputSection()
{
	if (UMovieSceneDatasetInputSection* Existing = GetInputSection())
	{
		// One take, one section. Returning the existing section rather than adding another
		// keeps a take's input from being split across two sections, which would leave the
		// derived sidecar missing whichever half the reader did not find.
		return Existing;
	}

	UMovieSceneDatasetInputSection* NewInputSection = CastChecked<UMovieSceneDatasetInputSection>(CreateNewSection());
	AddSection(*NewInputSection);

	return NewInputSection;
}

UMovieSceneDatasetInputSection* UMovieSceneDatasetInputTrack::GetInputSection() const
{
	for (UMovieSceneSection* Section : Sections)
	{
		if (UMovieSceneDatasetInputSection* InputSection = Cast<UMovieSceneDatasetInputSection>(Section))
		{
			return InputSection;
		}
	}

	return nullptr;
}

bool UMovieSceneDatasetInputTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	// Exact type, not IsChildOf in the permissive direction: a plain UMovieSceneSection
	// carries no input channels, and accepting one would leave a track that looks populated
	// and reads back nothing.
	return SectionClass == UMovieSceneDatasetInputSection::StaticClass();
}

UMovieSceneSection* UMovieSceneDatasetInputTrack::CreateNewSection()
{
	return NewObject<UMovieSceneDatasetInputSection>(this, NAME_None, RF_Transactional);
}

void UMovieSceneDatasetInputTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}

void UMovieSceneDatasetInputTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}

void UMovieSceneDatasetInputTrack::RemoveSectionAt(int32 SectionIndex)
{
	Sections.RemoveAt(SectionIndex);
}

bool UMovieSceneDatasetInputTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}

const TArray<UMovieSceneSection*>& UMovieSceneDatasetInputTrack::GetAllSections() const
{
	return Sections;
}

bool UMovieSceneDatasetInputTrack::IsEmpty() const
{
	return Sections.IsEmpty();
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneDatasetInputTrack::GetDisplayName() const
{
	return LOCTEXT("TrackName", "Dataset Input");
}
#endif

#undef LOCTEXT_NAMESPACE

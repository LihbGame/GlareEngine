#pragma once

#include <tuple>
#include <vector>

#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>
  
	
//从一组动画轨道计算给定时间的转换。不直接用，最好使用AnimPlayer类。
class AnimEvaluator {
public:
	//给定动画的构造方法。 动画在对象的整个生命周期中都是固定的。
	// @param pAnim   为其计算姿势的动画。 动画对象的所有权归调用者所有。
	AnimEvaluator(const aiAnimation* pAnim);

	//The class destructor.
	~AnimEvaluator();

	//计算给定时间的动画轨道。随后可以通过调用GetTransformations()将计算出的姿态作为转换矩阵数组进行检索。
	//@param pTime您要计算动画的时间，以秒为单位。 将被映射到动画周期中，因此它可以是任意值。 最好使用不断增加的时间。
	void Evaluate(double pTime);

	//返回在上一次Evaluate()调用时计算的变换矩阵。 该数组与aiAnimation的mChannels数组匹配。
	const std::vector<aiMatrix4x4>& GetTransformations() const { return mTransforms; }

protected:
	const aiAnimation* mAnim;
	double mLastTime;
	std::vector<std::tuple<unsigned int, unsigned int, unsigned int> > mLastPositions;
	std::vector<aiMatrix4x4> mTransforms;
};